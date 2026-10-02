#!/usr/bin/env python3
"""Reject proprietary game artifacts and wholesale binary-derived exports.

The staged mode is used by the composite pre-commit hook. The tree mode is a
CI backstop that scans every tracked file after checkout. The detector is
deliberately conservative about source and small synthetic fixtures; it blocks
known artifact paths/extensions, binary signatures, raw-capture naming, and
large decompiler-style text corpora.
"""

from __future__ import annotations

import argparse
import gzip
import hashlib
import io
import json
import re
import struct
import subprocess
import sys
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
BLOCKED_SUFFIXES = {
    ".xbe", ".xex", ".exe", ".dll", ".pdb", ".dbg", ".sym", ".obj",
    ".iso", ".xiso", ".yelo", ".dmp", ".dump", ".core",
    ".savestate", ".ram", ".halorec", ".raw", ".xpr", ".xmv", ".bik",
    ".xwb", ".xsb", ".bitmap", ".sound", ".scenario", ".yelo",
    ".7z", ".rar", ".xz", ".bz2", ".zst", ".bin", ".xvc",
    ".weapon", ".vehicle", ".biped", ".scenery", ".gbxmodel",
    ".model", ".collision_model", ".scenario_structure_bsp", ".font",
}
BLOCKED_EXACT = {
    "halo_2276_functions.txt",
    "tools/equivalence/_global_bytes.py",
    "tools/equivalence/known_globals.json",
    "ci/snapshot.json",
}
BLOCKED_PREFIXES = ("halo-patched/", "delinked/", "halo_decompiled/",
                    "tests/golden/periodic_tables/")
SAFE_EXACT = {
    "css/bootstrap.min.css.map",
    "artifacts/snapshots/d1f40_synthetic.json",
    "tools/equivalence/snapshot_verify_policy.json",
}
RAW_CAPTURE_WORDS = re.compile(
    r"(?:^|[/_.-])(?:ram|gamestate|game_state|savestate|"
    r"snapshot|glob_[0-9a-f]+|rdata_[0-9a-f]+)(?:$|[/_.-])", re.I
)
DECOMPILER_SYMBOL = re.compile(rb"\b(?:FUN|LAB|sub|loc)_[0-9A-Fa-f]{6,16}\b")
REVIEWED_SYNTHETIC = json.loads(
    (ROOT / "tools/audit/reviewed_synthetic_fixtures.json").read_text(encoding="utf-8")
)["sha256"]


def path_reason(path: str) -> str | None:
    normalized = path.replace("\\", "/")
    while normalized.startswith("./"):
        normalized = normalized[2:]
    lower = normalized.lower()
    if lower in BLOCKED_EXACT:
        return "known binary-derived corpus"
    if lower in {"delinked/manifest.json", "halo_decompiled/readme.md",
                 "halo_decompiled/index.jsonl"}:
        return None  # Metadata is still inspected by content_reason.
    if lower.startswith(BLOCKED_PREFIXES):
        return "reserved local/proprietary artifact directory"
    if any(lower.endswith(suffix) for suffix in BLOCKED_SUFFIXES):
        return "blocked executable, symbol, object, image, or dump extension"
    if lower.endswith(".map") and not lower.endswith(".css.map"):
        return "game/linker map files are not public-repository inputs"
    if lower.endswith(".bin") and RAW_CAPTURE_WORDS.search(lower):
        return "raw memory/game-state capture naming"
    synthetic = (normalized in SAFE_EXACT or
                 (lower.startswith("tools/equivalence/regression_snapshots/") and
                  lower.endswith(".json")))
    if lower.endswith(".json") and RAW_CAPTURE_WORDS.search(lower) and not synthetic:
        return "raw memory/game-state snapshot naming"
    return None


def content_reason(path: str, data: bytes, depth: int = 0) -> str | None:
    if data.startswith(b"XBEH"):
        return "XBE file signature"
    if data.startswith((b"Microsoft C/C++ MSF", b"Microsoft C/C++ program database")):
        return "Microsoft PDB signature"
    if data.startswith(b"MZ"):
        return "PE executable signature"
    if data.startswith((b"MDMP", b"HMRC")) or (
            len(data) >= 2048 and data[:4] == b"head" and data[2044:2048] == b"foot"):
        return "game map, minidump, or memory-recording signature"
    if len(data) >= 20 and data[:2] in (b"\x4c\x01", b"\x64\x86"):
        machine, sections, _, symbols, count, optional_size, _ = struct.unpack_from(
            "<HHIIIHH", data)
        if (machine and 0 < sections < 200 and optional_size == 0 and
                20 + sections * 40 <= len(data) and
                (symbols == 0 or symbols + count * 18 <= len(data))):
            return "COFF object signature (keep generated objects local)"
    if data.startswith(b"!<arch>\n"):
        return "static-library/object package (keep compiled packages local)"

    # Inspect renamed ZIP/gzip containers too. Fail closed on unsupported,
    # encrypted, nested-too-deep, or oversized containers; no public fixture
    # needs a large compressed proprietary payload.
    archive = data.startswith((b"PK\x03\x04", b"PK\x05\x06", b"\x1f\x8b"))
    if archive:
        if depth >= 3:
            return "archive nesting exceeds inspection limit"
        limit = 16 * 1024 * 1024
        try:
            if data.startswith(b"\x1f\x8b"):
                with gzip.GzipFile(fileobj=io.BytesIO(data)) as stream:
                    payload = stream.read(limit + 1)
                if len(payload) > limit:
                    return "compressed payload exceeds inspection limit"
                return content_reason(path.removesuffix(".gz"), payload, depth + 1)
            with zipfile.ZipFile(io.BytesIO(data)) as package:
                members = package.infolist()
                if len(members) > 1000 or sum(m.file_size for m in members) > limit:
                    return "archive exceeds inspection limit"
                for member in members:
                    if member.is_dir():
                        continue
                    reason = path_reason(member.filename)
                    if not reason:
                        reason = content_reason(member.filename, package.read(member), depth + 1)
                    if reason:
                        return f"archive member {member.filename}: {reason}"
        except (OSError, RuntimeError, ValueError, zipfile.BadZipFile, EOFError) as exc:
            return f"archive could not be inspected: {type(exc).__name__}"
    if data.startswith((b"7z\xbc\xaf\x27\x1c", b"Rar!", b"\xfd7zXZ\x00", b"BZh", b"\x28\xb5\x2f\xfd")):
        return "unsupported compressed package; publish unpacked reviewed source"

    lower = path.lower()
    if lower.endswith((".json", ".jsonl")):
        sample = data[:8 * 1024 * 1024]
        if b'"regions"' in sample and (
                b'"captured_at"' in sample or b"live snapshot" in sample or
                b"converted from artifacts/snapshots" in sample):
            return "recorded live memory/game-state capture"
        if re.search(rb'"(?:0x)?[0-9A-Fa-f]{1,16}"\s*:\s*"[0-9A-Fa-f]{4096}', sample):
            if depth or REVIEWED_SYNTHETIC.get(path) != hashlib.sha256(data).hexdigest():
                return "hex-encoded raw memory/game-state capture"
        chunks = re.findall(rb'"0x[0-9A-Fa-f]{1,16}"\s*:\s*"([0-9A-Fa-f]{8,})"', sample)
        if sum(len(chunk)//2 for chunk in chunks) >= 4096:
            if depth or REVIEWED_SYNTHETIC.get(path) != hashlib.sha256(data).hexdigest():
                return "fragmented hex-encoded raw memory/game-state corpus"

    if len(data) >= 256 * 1024:
        sample = data[:8 * 1024 * 1024]
        symbol_count = len(DECOMPILER_SYMBOL.findall(sample))
        if symbol_count >= 10000:
            return f"wholesale decompiler/disassembly export ({symbol_count}+ symbols)"
        if len(re.findall(rb"\b__(?:fastcall|thiscall|usercall|userpurge)\b", sample)) >= 200:
            return "large decompiler export with generated calling conventions"
    return None


def range_entries(base: str, head: str):
    """Inspect every introduced revision, including artifacts deleted later."""
    revision = head if base == "0" * 40 else f"{base}..{head}"
    raw = subprocess.check_output(
        ["git", "log", revision, "--raw", "-z", "--no-abbrev", "--no-renames",
         "--diff-merges=separate", "--root", "--format="], cwd=ROOT)
    tokens = raw.split(b"\0")
    seen = set()
    index = 0
    while index < len(tokens):
        token = tokens[index].strip(b"\n")
        if token.startswith(b":"):
            fields = token.split()
            path = tokens[index + 1].decode("utf-8", "surrogateescape")
            oid = fields[3].decode()
            index += 1
            if oid != "0" * 40 and (path, oid) not in seen:
                seen.add((path, oid))
                yield path, oid
        index += 1


def scan_range(base: str, head: str) -> tuple[list[tuple[str, str]], int]:
    findings = []
    count = 0
    for path, oid in range_entries(base, head):
        count += 1
        reason = path_reason(path)
        if not reason:
            reason = content_reason(path, subprocess.check_output(
                ["git", "cat-file", "blob", oid], cwd=ROOT))
        if reason:
            findings.append((path, f"{reason} (blob {oid})"))
    return findings, count


def staged_paths() -> list[str]:
    proc = subprocess.run(
        ["git", "diff", "--cached", "--name-only", "--diff-filter=ACMR", "-z"],
        cwd=ROOT, check=True, stdout=subprocess.PIPE,
    )
    return [p.decode("utf-8", "surrogateescape")
            for p in proc.stdout.split(b"\0") if p]


def tracked_paths() -> list[str]:
    proc = subprocess.run(
        ["git", "ls-files", "-z"], cwd=ROOT, check=True, stdout=subprocess.PIPE,
    )
    return [p.decode("utf-8", "surrogateescape")
            for p in proc.stdout.split(b"\0") if p]


def staged_blob(path: str) -> bytes:
    return subprocess.check_output(["git", "show", ":" + path], cwd=ROOT)


def working_blob(path: str) -> bytes:
    return (ROOT / path).read_bytes()


def scan(paths: list[str], staged: bool) -> list[tuple[str, str]]:
    findings = []
    for path in paths:
        if not staged and not (ROOT / path).is_file():
            # A locally deleted tracked file remains in `git ls-files` until
            # its deletion is committed; CI checkouts never hit this case.
            continue
        reason = path_reason(path)
        if reason:
            findings.append((path, reason))
            continue
        try:
            data = staged_blob(path) if staged else working_blob(path)
        except (OSError, subprocess.CalledProcessError) as exc:
            findings.append((path, f"could not inspect tracked content: {exc}"))
            continue
        reason = content_reason(path, data)
        if reason:
            findings.append((path, reason))
    return findings


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--staged", action="store_true",
                      help="scan staged additions/modifications/renames")
    mode.add_argument("--tree", action="store_true",
                      help="scan all files tracked in the working tree")
    mode.add_argument("--range", nargs=2, metavar=("BASE", "HEAD"),
                      help="scan every introduced blob across new commits")
    args = parser.parse_args()

    if args.range:
        findings, count = scan_range(*args.range)
    else:
        paths = staged_paths() if args.staged else tracked_paths()
        findings = scan(paths, staged=args.staged)
        count = len(paths)
    if not findings:
        print(f"provenance artifact guard: PASS ({count} files/revisions inspected)")
        return 0

    print("provenance artifact guard: FAIL", file=sys.stderr)
    for path, reason in findings:
        print(f"  {path}: {reason}", file=sys.stderr)
    print("Keep these files outside Git and publish only independently derived "
          "metadata or reproducible measurements.", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
