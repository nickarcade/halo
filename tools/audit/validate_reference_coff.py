#!/usr/bin/env python3
"""Fail-closed validation for delinked reference COFF objects.

This is an opt-in guard for a future strict-COFF lane. VC71 scoring and
Unicorn equivalence continue to use the pristine XBE and function_bounds.json;
this module never changes either authority.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
from pathlib import Path
from typing import Any, Iterable, Optional

COFF_HEADER = struct.Struct("<HHIIIHH")
SECTION_HEADER = struct.Struct("<8sIIIIIIHHI")
SYMBOL = struct.Struct("<8sIhHBB")
RELOC_SIZE = 10
I386 = 0x14C
FUNCTION = 0x20
RELOC_WIDTH = {0x0006: 4, 0x0014: 4, 0x000A: 2, 0x000B: 4}
PAD_MNEMONICS = {"nop", "nopw", "nopl", "nopq", "int3"}


class InvalidReference(ValueError):
    """A reference cannot be used by a strict COFF consumer."""


def _sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _canonical(name: str) -> str:
    return re.sub(r"@\d+$", "", name.lstrip("_@"))


def _name(raw: bytes, strings: bytes) -> str:
    zeroes, offset = struct.unpack("<II", raw)
    if zeroes == 0:
        if offset < 4 or offset >= len(strings):
            raise InvalidReference("invalid COFF string-table name offset 0x%x" % offset)
        end = strings.find(b"\0", offset)
        if end < 0:
            raise InvalidReference("unterminated COFF string-table name")
        return strings[offset:end].decode("ascii", "replace")
    return raw.rstrip(b"\0").decode("ascii", "replace")


def _range_ok(start: int, size: int, total: int) -> bool:
    return 0 <= start <= total and 0 <= size <= total - start


def parse_coff(path: Path) -> dict[str, Any]:
    """Parse every table and relocation, rejecting any out-of-file range."""
    data = path.read_bytes()
    if len(data) < COFF_HEADER.size:
        raise InvalidReference("COFF header is truncated")
    machine, nsections, _stamp, sym_off, nsymbols, opt_size, _chars = COFF_HEADER.unpack_from(data)
    if machine != I386:
        raise InvalidReference("unexpected COFF machine 0x%04x" % machine)
    section_off = COFF_HEADER.size + opt_size
    if not _range_ok(section_off, nsections * SECTION_HEADER.size, len(data)):
        raise InvalidReference("COFF section-header table is truncated")
    if not _range_ok(sym_off, nsymbols * SYMBOL.size, len(data)):
        raise InvalidReference("COFF symbol table is truncated")
    string_off = sym_off + nsymbols * SYMBOL.size
    if not _range_ok(string_off, 4, len(data)):
        raise InvalidReference("COFF string-table length is truncated")
    string_size = struct.unpack_from("<I", data, string_off)[0]
    if string_size < 4 or not _range_ok(string_off, string_size, len(data)):
        raise InvalidReference("COFF string table has an invalid size")
    strings = data[string_off:string_off + string_size]

    sections = []
    for i in range(nsections):
        at = section_off + i * SECTION_HEADER.size
        raw_name, vsize, vaddr, raw_size, raw_off, reloc_off, _line_off, nreloc, _nline, flags = SECTION_HEADER.unpack_from(data, at)
        section_name = (_name(raw_name, strings) if raw_name[:4] == b"\0\0\0\0"
                        else raw_name.rstrip(b"\0").decode("ascii", "replace"))
        if raw_size and not _range_ok(raw_off, raw_size, len(data)):
            raise InvalidReference("section %s raw data is truncated" % section_name)
        raw_end = raw_off + raw_size
        if nreloc:
            if reloc_off == 0 or not _range_ok(reloc_off, nreloc * RELOC_SIZE, len(data)):
                raise InvalidReference("section %s relocation table is truncated" % section_name)
            if reloc_off < raw_end:
                raise InvalidReference("section %s relocation table overlaps raw data" % section_name)
            if reloc_off + nreloc * RELOC_SIZE > sym_off:
                raise InvalidReference("section %s relocation table overlaps symbol table" % section_name)
        elif raw_end > sym_off:
            raise InvalidReference("section %s raw data overlaps symbol table" % section_name)
        elif reloc_off and reloc_off > len(data):
            raise InvalidReference("section %s has an invalid relocation offset" % section_name)
        sections.append({"name": section_name, "vsize": vsize, "vaddr": vaddr,
                         "raw_size": raw_size, "raw_off": raw_off,
                         "reloc_off": reloc_off, "nreloc": nreloc,
                         "flags": flags})

    symbols = []
    index = 0
    while index < nsymbols:
        at = sym_off + index * SYMBOL.size
        raw_name, value, section, typ, storage, aux = SYMBOL.unpack_from(data, at)
        if index + 1 + aux > nsymbols:
            raise InvalidReference("COFF auxiliary symbol records are truncated")
        entry = {"index": index, "name": _name(raw_name, strings),
                 "value": value, "section": section, "type": typ,
                 "storage": storage, "aux": aux, "total_size": None}
        if aux and typ == FUNCTION:
            entry["total_size"] = struct.unpack_from("<I", data, at + SYMBOL.size + 4)[0]
        symbols.append(entry)
        index += 1 + aux

    relocations = []
    for sec_index, sec in enumerate(sections, 1):
        for i in range(sec["nreloc"]):
            at = sec["reloc_off"] + i * RELOC_SIZE
            virtual, sym_index, typ = struct.unpack_from("<IIH", data, at)
            width = RELOC_WIDTH.get(typ, 4)
            if virtual > sec["raw_size"] or width > sec["raw_size"] - virtual:
                raise InvalidReference("section %s relocation %d points outside raw data" % (sec["name"], i))
            if sym_index >= nsymbols:
                raise InvalidReference("section %s relocation %d has invalid symbol index %d" % (sec["name"], i, sym_index))
            relocations.append({"section": sec_index, "offset": virtual,
                                "symbol_index": sym_index, "type": typ})

    return {"data": data, "sections": sections, "symbols": symbols,
            "relocations": relocations, "sha256": _sha256(data)}


def _normalized(code: bytes) -> list[str]:
    """Return a relocation-insensitive instruction sequence."""
    try:
        import capstone
    except ImportError as exc:
        raise InvalidReference("capstone is required for reconciliation") from exc
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    insns = list(md.disasm(code, 0))
    if sum(i.size for i in insns) != len(code):
        raise InvalidReference("code does not disassemble to its complete extent")
    result = []
    for ins in insns:
        operands = re.sub(r"0x[0-9a-f]+", "IMM", ins.op_str.lower())
        operands = re.sub(r"(?<![a-z])[-+]?\d+", "IMM", operands)
        result.append(ins.mnemonic.lower() + (" " + operands if operands else ""))
    return result


def _trim_padding(insns: list[str]) -> list[str]:
    result = list(insns)
    while result and result[-1].split(" ", 1)[0] in PAD_MNEMONICS:
        result.pop()
    return result


def _function_extent(parsed: dict[str, Any], symbol: dict[str, Any]) -> tuple[int, int]:
    section_number = symbol["section"]
    if not 1 <= section_number <= len(parsed["sections"]):
        raise InvalidReference("function %s references invalid section %d" %
                               (symbol["name"], section_number))
    section = parsed["sections"][section_number - 1]
    start = symbol["value"]
    if start >= section["raw_size"]:
        raise InvalidReference("function %s starts outside its section" % symbol["name"])
    same_section = [s for s in parsed["symbols"]
                    if s["section"] == section_number and s["type"] == FUNCTION]
    if sum(s["value"] == start for s in same_section) != 1:
        raise InvalidReference("function %s has ambiguous duplicate start" % symbol["name"])
    peers = [s["value"] for s in same_section if s["value"] > start]
    if symbol["total_size"]:
        end = start + symbol["total_size"]
        if peers and min(peers) < end:
            raise InvalidReference("function %s extent overlaps a following function" % symbol["name"])
    else:
        # Without an auxiliary TotalSize, a multi-function section has no
        # authoritative per-symbol extent. Never infer one from a neighbour.
        if start != 0 or len(same_section) != 1:
            raise InvalidReference("function %s has no unambiguous COFF extent" % symbol["name"])
        end = section["raw_size"]
    if end <= start or end > section["raw_size"]:
        raise InvalidReference("function %s has invalid extent" % symbol["name"])
    return start, end


def _coff_symbols(parsed: dict[str, Any], name: str) -> list[dict[str, Any]]:
    wanted = _canonical(name)
    return [s for s in parsed["symbols"] if s["section"] > 0 and s["type"] == FUNCTION
            and _canonical(s["name"]) == wanted]


def validate_functions(path: Path, references: Iterable[dict[str, Any]]) -> dict[str, Any]:
    """Validate selected functions against verified raw-XBE spans.

    A reference item contains ``name``, ``address``, ``bytes``, and optional
    ``bound_provenance``. The caller owns verification of the raw-XBE bytes.
    """
    result: dict[str, Any] = {
        "schema_version": 1,
        "authority": "raw_xbe",
        "reference": {"path": str(path), "sha256": None},
        "checks": {"coff_structure": "pending", "relocations": "pending"},
        "functions": [], "valid": False, "usable_for_strict_coff": False,
        "reasons": [],
    }
    try:
        parsed = parse_coff(path)
        result["reference"]["sha256"] = parsed["sha256"]
        result["checks"]["coff_structure"] = "valid"
        result["checks"]["relocations"] = "valid"
    except (OSError, InvalidReference) as exc:
        result["reasons"].append({"code": "invalid_coff", "detail": str(exc)})
        return result

    all_valid = True
    for ref in references:
        name = str(ref["name"])
        entry: dict[str, Any] = {"name": name, "address": "0x%08x" % int(ref["address"]),
                                 "valid": False}
        matches = _coff_symbols(parsed, name)
        if len(matches) != 1:
            entry.update({"reason": "missing_or_ambiguous_function_symbol",
                          "symbol_matches": len(matches)})
            all_valid = False
            result["functions"].append(entry)
            continue
        try:
            symbol = matches[0]
            start, end = _function_extent(parsed, symbol)
            section = parsed["sections"][symbol["section"] - 1]
            code = parsed["data"][section["raw_off"] + start:section["raw_off"] + end]
            expected = bytes(ref["bytes"])
            coff_norm = _trim_padding(_normalized(code))
            raw_norm = _trim_padding(_normalized(expected))
            entry.update({"coff_symbol": symbol["name"], "coff_section": section["name"],
                          "coff_offset": start, "coff_end": end,
                          "coff_length": len(code), "raw_length": len(expected),
                          "raw_bound_end": "0x%08x" % (int(ref["address"]) + len(expected)),
                          "relocation_count": sum(1 for r in parsed["relocations"]
                                                   if r["section"] == symbol["section"]
                                                   and start <= r["offset"] < end),
                          "candidate_sha256": _sha256(code),
                          "raw_span_sha256": _sha256(expected),
                          "normalized_sha256": _sha256("\n".join(coff_norm).encode()),
                          "raw_normalized_sha256": _sha256("\n".join(raw_norm).encode()),
                          "bound_provenance": ref.get("bound_provenance", "raw_xbe")})
            if coff_norm != raw_norm:
                entry["reason"] = "normalized_code_does_not_match_raw_xbe_bound"
                all_valid = False
            else:
                entry["valid"] = True
        except InvalidReference as exc:
            entry["reason"] = str(exc)
            all_valid = False
        result["functions"].append(entry)

    result["valid"] = all_valid and bool(result["functions"])
    result["usable_for_strict_coff"] = result["valid"]
    if not result["valid"]:
        result["reasons"].append({"code": "function_validation_failed"})
    return result


def function_names(path: Path) -> set[str]:
    parsed = parse_coff(path)
    return {_canonical(s["name"]) for s in parsed["symbols"]
            if s["section"] > 0 and s["type"] == FUNCTION}


def references_from_xbe(path: Path, bounds_path: Path, names: Optional[set[str]] = None) -> list[dict[str, Any]]:
    """Build raw-XBE spans from function_bounds.json without changing its authority."""
    import sys
    sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "equivalence"))
    import xbe_image
    bounds = json.loads(bounds_path.read_text(encoding="utf-8"))
    xbe_image.assert_pristine(path)
    raw, sections = xbe_image.load_xbe(path)
    refs = []
    xbe_hash = _sha256(raw)
    bounds_hash = _sha256(bounds_path.read_bytes())
    for key, bound in bounds.items():
        if key == "_meta" or not isinstance(bound, dict):
            continue
        address = int(key, 16)
        name = bound.get("name") or "FUN_%08x" % address
        if names and name not in names and ("FUN_%08x" % address) not in names:
            continue
        end = int(bound["end"], 16)
        section = next((s for s in sections if s.va <= address < s.va + s.raw_size), None)
        if section is None or end <= address or end > section.va + section.raw_size:
            raise InvalidReference("raw-XBE bound %s is outside a file-backed section" % key)
        offset = section.raw_off + address - section.va
        refs.append({"name": name, "address": address,
                     "bytes": raw[offset:offset + end - address],
                     "bound_provenance": "function_bounds.json",
                     "raw_xbe_sha256": xbe_hash,
                     "bounds_sha256": bounds_hash})
    return refs


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--object", required=True, type=Path)
    parser.add_argument("--xbe", type=Path, default=Path("halo-patched/cachebeta.xbe"))
    parser.add_argument("--bounds", type=Path, default=Path("tools/verify/function_bounds.json"))
    parser.add_argument("--name", action="append")
    parser.add_argument("--json", action="store_true", help="reserved for stable machine-readable output")
    parser.add_argument("--check", action="store_true", help="fail unless strict-valid")
    args = parser.parse_args()
    try:
        names = set(args.name) if args.name else function_names(args.object)
        refs = references_from_xbe(args.xbe, args.bounds, names)
        result = validate_functions(args.object, refs)
        if refs:
            result["raw_xbe"] = {"path": str(args.xbe),
                                 "sha256": refs[0].get("raw_xbe_sha256")}
            result["bounds"] = {"path": str(args.bounds),
                                 "sha256": refs[0].get("bounds_sha256"),
                                 "authority": "function_bounds.json"}
    except (OSError, InvalidReference, ValueError) as exc:
        result = {"schema_version": 1, "authority": "raw_xbe", "valid": False,
                  "usable_for_strict_coff": False,
                  "reasons": [{"code": "input_invalid", "detail": str(exc)}]}
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0 if not args.check or result.get("usable_for_strict_coff") else 1


if __name__ == "__main__":
    raise SystemExit(main())
