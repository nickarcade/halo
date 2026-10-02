#!/usr/bin/env python3
"""extract_fn.py — Query and extract decompiled functions from the Halo CE corpus.

Integrates:
  - halo_decompiled/index.jsonl        (byte offsets into cachebeta.elf.c)
  - halo_decompiled/corpus_metadata.json (11,137 cataloged symbols, VAs, sizes)
  - halo_decompiled/bge_m3_embeddings.npy (1024-dim neural embeddings for similarity)
  - halo_decompiled/cachebeta.elf.c    (authoritative decompiled C source)
  - halo_decompiled/cachebeta.elf.asm  (x86 assembly and stack frames)

Usage:
  # Extract by address (supports 0x1c4990, 1c4990, 0x001c4990)
  python3 tools/extract_fn.py 0x1c4990

  # Extract by exact symbol name
  python3 tools/extract_fn.py playlist_profile_new

  # Keyword search across symbol names in corpus_metadata.json
  python3 tools/extract_fn.py playlist_profile
  python3 tools/extract_fn.py "checkpoint"

  # Find semantically/structurally similar functions via BGE-M3 embeddings
  python3 tools/extract_fn.py --similar 0x1c1e20 -k 5

  # Display function metadata only without dumping source
  python3 tools/extract_fn.py --info "playlist_profile"

  # Include x86 disassembly and stack frames from cachebeta.elf.asm
  python3 tools/extract_fn.py 0x1c4990 --asm

  # Extract all matching functions when multiple matches exist
  python3 tools/extract_fn.py --all "screen_flash"
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
from pathlib import Path


def find_repo_root() -> Path:
    cur = Path(__file__).resolve().parent
    while cur != cur.parent:
        if (cur / "halo_decompiled").is_dir() or (cur / "kb.json").is_file():
            return cur
        cur = cur.parent
    return Path.cwd()


REPO_ROOT = find_repo_root()
DECOMPILED_DIR = REPO_ROOT / "halo_decompiled"
INDEX_PATH = DECOMPILED_DIR / "index.jsonl"
METADATA_PATH = DECOMPILED_DIR / "corpus_metadata.json"
EMBEDDINGS_PATH = DECOMPILED_DIR / "bge_m3_embeddings.npy"
C_PATH = DECOMPILED_DIR / "cachebeta.elf.c"
ASM_PATH = DECOMPILED_DIR / "cachebeta.elf.asm"

_index_cache: dict[str, dict] | None = None
_metadata_cache: list[dict] | None = None
_embeddings_cache = None


def normalize_addr(query: str) -> str | None:
    """Normalize hex address strings like '0x1c4990', '1c4990', '0x001c4990'."""
    s = query.strip()
    if s.startswith("0x") or s.startswith("0X"):
        try:
            return hex(int(s, 16))
        except ValueError:
            return None
    # If pure hex string (at least 4 hex characters)
    if re.fullmatch(r"[0-9a-fA-F]{4,8}", s):
        try:
            return hex(int(s, 16))
        except ValueError:
            return None
    return None


def load_metadata() -> list[dict]:
    global _metadata_cache
    if _metadata_cache is None:
        if not METADATA_PATH.is_file():
            raise FileNotFoundError(f"Missing metadata file: {METADATA_PATH}")
        with open(METADATA_PATH, "r", encoding="utf-8") as f:
            _metadata_cache = json.load(f)
    return _metadata_cache


def load_index() -> dict[str, dict]:
    global _index_cache
    if _index_cache is None:
        if not INDEX_PATH.is_file():
            raise FileNotFoundError(f"Missing index file: {INDEX_PATH}")
        _index_cache = {}
        with open(INDEX_PATH, "r", encoding="utf-8") as f:
            for line in f:
                if not line.strip():
                    continue
                entry = json.loads(line)
                if not entry.get("error"):
                    _index_cache[entry["addr"]] = entry
    return _index_cache


def get_c_code(offset: int, length: int) -> str:
    if not C_PATH.is_file():
        return f"<decompiled source missing: {C_PATH}>"
    with open(C_PATH, "rb") as cf:
        cf.seek(offset)
        return cf.read(length).decode("utf-8", errors="replace")


def get_asm_code(addr_norm: str, max_lines: int = 120) -> str | None:
    if not ASM_PATH.is_file():
        return None
    target_tag = f"; {addr_norm}"
    target_tag_upper = f"; 0x{int(addr_norm, 16):08x}"
    lines = []
    collecting = False

    with open(ASM_PATH, "r", encoding="utf-8", errors="replace") as f:
        for line in f:
            if not collecting:
                if target_tag in line.lower() or target_tag_upper in line.lower():
                    collecting = True
                    lines.append(line)
            else:
                if line.startswith("; ") and ("; 0x" in line or "; sub_" in line):
                    break
                lines.append(line)
                if len(lines) >= max_lines:
                    lines.append("... [truncated additional disassembly]\n")
                    break

    return "".join(lines) if lines else None


def find_similar(target_query: str, top_k: int = 5) -> list[tuple[float, dict]]:
    try:
        import numpy as np
    except ImportError:
        print("numpy is required for similarity search (run: pip install numpy)", file=sys.stderr)
        return []

    if not EMBEDDINGS_PATH.is_file():
        print(f"Embeddings file missing: {EMBEDDINGS_PATH}", file=sys.stderr)
        return []

    metadata = load_metadata()
    global _embeddings_cache
    if _embeddings_cache is None:
        _embeddings_cache = np.load(EMBEDDINGS_PATH)

    addr_norm = normalize_addr(target_query)
    target_idx = None

    if addr_norm:
        for idx, m in enumerate(metadata):
            if m["addr"] == addr_norm:
                target_idx = idx
                break

    if target_idx is None:
        q_lower = target_query.strip().lower()
        for idx, m in enumerate(metadata):
            if m["name"].lower() == q_lower or m["name"].lower().startswith(q_lower + "("):
                target_idx = idx
                break

    if target_idx is None:
        print(f"Target '{target_query}' not found in corpus_metadata.json for similarity search.", file=sys.stderr)
        return []

    target_vec = _embeddings_cache[target_idx]
    sims = np.dot(_embeddings_cache, target_vec)
    ranked = np.argsort(sims)[::-1]

    results = []
    for idx in ranked:
        if idx == target_idx and len(results) > 0:
            continue
        results.append((float(sims[idx]), metadata[idx]))
        if len(results) >= top_k:
            break

    return results


def search_metadata(query: str) -> list[dict]:
    metadata = load_metadata()
    addr_norm = normalize_addr(query)

    # 1. Exact address match
    if addr_norm:
        matches = [m for m in metadata if m["addr"] == addr_norm]
        if matches:
            return matches

    q_lower = query.strip().lower()

    # 2. Exact name match
    exact_matches = [
        m for m in metadata
        if m["name"].lower() == q_lower or m["name"].lower() == f"{q_lower}()"
    ]
    if exact_matches:
        return exact_matches

    # 3. Substring / regex search
    try:
        rx = re.compile(q_lower, re.IGNORECASE)
        matches = [m for m in metadata if rx.search(m["name"]) or rx.search(m["addr"])]
    except re.error:
        matches = [m for m in metadata if q_lower in m["name"].lower() or q_lower in m["addr"].lower()]

    return matches


def print_function(entry: dict, show_asm: bool = False):
    addr = entry["addr"]
    name = entry.get("name", "<unnamed>")
    size = entry.get("size", 0)
    seq = entry.get("seq", "?")

    index = load_index()
    idx_entry = index.get(addr)

    print(f"=== {name} @ {addr} ({size} bytes, seq {seq}) ===")

    if idx_entry:
        c_code = get_c_code(idx_entry["c_offset"], idx_entry["c_len"])
        print(c_code.strip())
    else:
        print(f"<Warning: function address {addr} not mapped in index.jsonl>")

    if show_asm:
        asm_code = get_asm_code(addr)
        if asm_code:
            print("\n--- Disassembly & Stack Frame (cachebeta.elf.asm) ---")
            print(asm_code.strip())
    print()


def main():
    parser = argparse.ArgumentParser(
        description="Query and extract decompiled functions from the Halo CE corpus.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument("query", nargs="*", help="Function address (0x1c4990) or symbol name/keyword")
    parser.add_argument("-s", "--search", help="Explicit keyword search across function symbols")
    parser.add_argument("-a", "--all", action="store_true", help="Extract all matching functions when multiple match")
    parser.add_argument("-i", "--info", action="store_true", help="Show function metadata table only without source code")
    parser.add_argument("--asm", action="store_true", help="Include x86 disassembly and stack layout from cachebeta.elf.asm")
    parser.add_argument("--similar", metavar="TARGET", help="Find semantically similar functions via BGE-M3 embeddings")
    parser.add_argument("-k", "--top-k", type=int, default=5, help="Number of similar functions to return (default: 5)")

    args = parser.parse_args()

    # Similarity mode
    if args.similar:
        results = find_similar(args.similar, top_k=args.top_k)
        if results:
            print(f"\nTop {len(results)} semantically similar functions to '{args.similar}':")
            print(f"  {'Score':7} | {'Address':10} | {'Size':6} | {'Seq':5} | {'Function Symbol'}")
            print("  " + "-" * 75)
            for score, m in results:
                print(f"  {score:6.4f}  | {m['addr']:10} | {m['size']:5}B | {m['seq']:5} | {m['name']}")
            print()
            if not args.info and len(results) > 0 and args.all:
                for _, m in results:
                    print_function(m, show_asm=args.asm)
        return

    queries = list(args.query)
    if args.search:
        queries.append(args.search)

    if not queries:
        parser.print_help()
        return

    for q in queries:
        matches = search_metadata(q)

        # Fallback check directly in index.jsonl if not found in metadata
        if not matches:
            addr_norm = normalize_addr(q)
            if addr_norm:
                index = load_index()
                if addr_norm in index:
                    matches = [index[addr_norm]]

        if not matches:
            print(f"No functions found matching '{q}'.", file=sys.stderr)
            continue

        if len(matches) == 1 or args.all:
            for m in matches:
                if args.info:
                    print(f"{m['addr']:10} | {m.get('size', 0):5}B | seq {m.get('seq', '?'):5} | {m['name']}")
                else:
                    print_function(m, show_asm=args.asm)
        else:
            print(f"Found {len(matches)} matching functions in corpus_metadata.json for '{q}':")
            print(f"  {'Address':10} | {'Size':6} | {'Seq':5} | {'Function Symbol'}")
            print("  " + "-" * 65)
            for m in matches[:50]:
                print(f"  {m['addr']:10} | {m.get('size', 0):5}B | {m.get('seq', '?'):5} | {m['name']}")
            if len(matches) > 50:
                print(f"  ... and {len(matches) - 50} more matching functions.")
            print(f"\nTip: Extract a specific function with: python3 tools/extract_fn.py <addr>")
            print(f"     Or dump all {len(matches)} matches with: python3 tools/extract_fn.py --all \"{q}\"\n")


if __name__ == "__main__":
    main()
