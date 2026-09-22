#!/usr/bin/env python3
"""Relocation-aware structural comparison of a candidate COFF function to raw XBE.

This is deliberately a separate lane from ``raw_byte_audit.py``. The strict
literal audit remains byte-for-byte and still rejects every candidate
relocation. This lane masks only candidate relocation operand fields after
checking that the pristine XBE has an unambiguous x86 operand at the same
location.

Only i386 COFF REL32 and DIR32 relocations are accepted. The raw XBE has no
relocation table, so direct E8/E9 targets and a conservative set of absolute
operand encodings are classified from bytes. Unknown encodings are reported as
uncomparable instead of being guessed.
"""

import argparse
import concurrent.futures
import hashlib
import json
import os
import re
import struct
import sys
from datetime import datetime, timezone
from difflib import SequenceMatcher
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(HERE))

import raw_byte_audit as strict
import vc71_verify as vc71
import xbe_reference as xref

COFF_FILE_HEADER = struct.Struct("<HHIIIHH")
COFF_SECTION_HEADER = struct.Struct("<8sIIIIIIHHI")
COFF_SYMBOL = struct.Struct("<8sIhHBB")
COFF_RELOCATION = struct.Struct("<IIH")
IMAGE_SYM_DTYPE_FUNCTION = 0x20
IMAGE_REL_I386_DIR32 = 0x0006
IMAGE_REL_I386_REL32 = 0x0014

RELOCATION_NAMES = {
    IMAGE_REL_I386_DIR32: "DIR32",
    IMAGE_REL_I386_REL32: "REL32",
}


def _sha256(data):
    return hashlib.sha256(data).hexdigest()


def _coff_name(raw, strings):
    zeroes, offset = struct.unpack("<II", raw)
    if zeroes == 0 and offset:
        if offset < 4 or offset >= len(strings):
            raise strict.NotComparable("COFF string-table name offset is invalid")
        return strings[offset:].split(b"\0", 1)[0].decode("ascii", "replace")
    return raw.split(b"\0", 1)[0].decode("ascii", "replace")


def _parse_coff(path, function):
    """Return function bytes, relocations, and extraction provenance."""
    data = Path(path).read_bytes()
    if len(data) < COFF_FILE_HEADER.size:
        raise strict.NotComparable("candidate is smaller than a COFF file header")
    machine, section_count, _stamp, symbol_offset, symbol_count, optional_size, _chars = (
        COFF_FILE_HEADER.unpack_from(data, 0))
    if machine != 0x14C:
        raise strict.NotComparable("candidate is not an i386 COFF object")
    section_offset = COFF_FILE_HEADER.size + optional_size
    section_end = section_offset + section_count * COFF_SECTION_HEADER.size
    if section_end > len(data):
        raise strict.NotComparable("candidate has truncated COFF section headers")
    symbol_end = symbol_offset + symbol_count * COFF_SYMBOL.size
    if symbol_end + 4 > len(data):
        raise strict.NotComparable("candidate has truncated COFF symbol table")
    string_size = struct.unpack_from("<I", data, symbol_end)[0]
    if string_size < 4 or symbol_end + string_size > len(data):
        raise strict.NotComparable("candidate has invalid COFF string table")
    strings = data[symbol_end:symbol_end + string_size]

    sections = []
    for i in range(section_count):
        raw = COFF_SECTION_HEADER.unpack_from(data, section_offset + i * COFF_SECTION_HEADER.size)
        name, _vsize, _va, raw_size, raw_offset, reloc_offset, _lo, reloc_count, _lc, _flags = raw
        if raw_offset + raw_size > len(data):
            raise strict.NotComparable("candidate has truncated section data")
        if reloc_count and reloc_offset + reloc_count * COFF_RELOCATION.size > len(data):
            raise strict.NotComparable("candidate has truncated relocation table")
        sections.append({"name": _coff_name(name, strings), "raw_size": raw_size,
                         "raw_offset": raw_offset, "reloc_offset": reloc_offset,
                         "reloc_count": reloc_count})

    symbols = []
    index = 0
    while index < symbol_count:
        at = symbol_offset + index * COFF_SYMBOL.size
        raw_name, value, section, typ, storage, aux_count = COFF_SYMBOL.unpack_from(data, at)
        if index + 1 + aux_count > symbol_count:
            raise strict.NotComparable("candidate has truncated COFF auxiliary symbols")
        symbols.append({"index": index, "name": _coff_name(raw_name, strings),
                        "value": value, "section": section, "type": typ,
                        "storage": storage, "aux_count": aux_count})
        index += 1 + aux_count

    matches = [s for s in symbols if s["section"] > 0 and
               s["type"] == IMAGE_SYM_DTYPE_FUNCTION and
               strict._same_symbol(s["name"], function)]
    if len(matches) != 1:
        raise strict.NotComparable("candidate has %d COFF function symbols for %s" %
                                  (len(matches), function))
    symbol = matches[0]
    section_index = symbol["section"] - 1
    if section_index >= len(sections):
        raise strict.NotComparable("candidate function symbol references no section")
    section = sections[section_index]
    start = symbol["value"]
    if start >= section["raw_size"]:
        raise strict.NotComparable("candidate function starts outside its section")

    # Match the strict audit's extent rules without rejecting relocations.
    symbol_at = symbol_offset + symbol["index"] * COFF_SYMBOL.size
    total_size = None
    if symbol["aux_count"] and symbol["type"] == IMAGE_SYM_DTYPE_FUNCTION:
        total_size = struct.unpack_from("<I", data, symbol_at + COFF_SYMBOL.size + 4)[0]
    if total_size:
        end = start + total_size
    else:
        funcs = [s for s in symbols if s["section"] == symbol["section"] and
                 s["type"] == IMAGE_SYM_DTYPE_FUNCTION]
        if start != 0 or len(funcs) != 1:
            raise strict.NotComparable("candidate function has no unambiguous COFF extent")
        end = section["raw_size"]
    if end <= start or end > section["raw_size"]:
        raise strict.NotComparable("candidate has invalid COFF function extent")

    relocs = []
    for i in range(section["reloc_count"]):
        at = section["reloc_offset"] + i * COFF_RELOCATION.size
        offset, symbol_index, reloc_type = COFF_RELOCATION.unpack_from(data, at)
        if start <= offset < end:
            target = next((s for s in symbols if s["index"] == symbol_index), None)
            relocs.append({"offset": offset - start, "absolute_offset": offset,
                           "symbol_index": symbol_index, "type": reloc_type,
                           "type_name": RELOCATION_NAMES.get(reloc_type, "UNKNOWN"),
                           "symbol": target})
    code = data[section["raw_offset"] + start:section["raw_offset"] + end]
    return code, relocs, {"coff_symbol": symbol["name"], "coff_section": section["name"],
                          "coff_offset": start, "coff_end": end,
                          "coff_relocations": section["reloc_count"]}


def _raw_name_for_target(target):
    entry = xref.bounds_entry(target)
    return entry.get("name") if entry else None


def _normal_name(name):
    if not name:
        return None
    name = name.lstrip("_@")
    if "@" in name and name.rsplit("@", 1)[1].isdigit():
        name = name.rsplit("@", 1)[0]
    return name


def _raw_addresses_for_name(name):
    """Return raw-XBE addresses whose bounds entry has this exact name."""
    normalized = _normal_name(name)
    matches = []
    for address, entry in xref._bounds().items():
        if _normal_name(entry.get("name")) == normalized:
            matches.append(address)
    return matches


def _target_identity(candidate_symbol, target, logical_target, convention, injected=None):
    """Resolve a COFF symbol and compare its logical target to raw XBE.

    For i386 COFF REL32, the relocation field is the addend A and the linker
    writes S + A - P into the linked image.  Therefore the candidate's logical
    target is S + A, not the candidate field bytes (which are not the raw
    displacement).  DIR32 uses the same logical S + A rule with an absolute
    linked operand.
    """
    if injected is not None:
        injected_target = injected.get("logical_target")
        if injected_target is None or injected_target != target:
            return {"status": "unresolved", "reason": "injected logical target differs from raw target",
                    "logical_target": injected_target, "raw": "0x%08x" % target}
        return {"status": "resolved", "method": "injected_logical_target",
                "logical_target": "0x%08x" % injected_target, "raw": "0x%08x" % target,
                "convention": convention}
    candidate_name = candidate_symbol.get("name") if candidate_symbol else None
    candidate_norm = _normal_name(candidate_name)
    fun = re.fullmatch(r"FUN_([0-9a-fA-F]{8})", candidate_norm or "")
    if fun:
        symbol_target = int(fun.group(1), 16)
        method = "candidate_FUN_address"
    else:
        addresses = _raw_addresses_for_name(candidate_name)
        if len(addresses) != 1:
            return {"status": "unresolved", "reason": "normal symbol lacks unique raw-XBE address backing",
                    "candidate": candidate_name, "raw": "0x%08x" % target,
                    "raw_name_addresses": ["0x%08x" % address for address in addresses],
                    "logical_target": "0x%08x" % logical_target}
        symbol_target = addresses[0]
        method = "unique_bounds_name"
    if logical_target != target:
        return {"status": "unresolved", "reason": "candidate symbol target differs from raw target",
                "candidate": candidate_name, "raw": "0x%08x" % target,
                "symbol_address": "0x%08x" % symbol_target,
                "logical_target": "0x%08x" % logical_target,
                "convention": convention}
    return {"status": "resolved", "method": method,
            "candidate": candidate_name, "raw": "0x%08x" % target,
            "symbol_address": "0x%08x" % symbol_target,
            "logical_target": "0x%08x" % logical_target,
            "convention": convention}


_FIELD_CACHE = {}


def _decode_fields(code):
    """Map each 4-byte relocatable operand field in ``code`` to its encoding.

    Capstone reports the exact offset and width of an instruction's
    displacement and immediate fields, so a relocation can be matched to the
    operand it actually patches instead of being guessed from a hand-written
    opcode whitelist.  A whitelist silently refuses every encoding it forgot,
    which is indistinguishable from a real mismatch.

    Returns ``None`` when Capstone is unavailable, and ``{}`` when the code
    does not decode, so callers stay fail-closed.
    """
    key = bytes(code)
    if key in _FIELD_CACHE:
        return _FIELD_CACHE[key]
    try:
        from capstone import CS_ARCH_X86, CS_GRP_CALL, CS_GRP_JUMP, CS_MODE_32, Cs
    except ImportError:
        return None
    decoder = Cs(CS_ARCH_X86, CS_MODE_32)
    decoder.detail = True
    fields = {}
    cursor = 0
    for instruction in decoder.disasm(key, 0):
        if instruction.address != cursor:
            break  # a gap means the linear sweep desynced; stop trusting it
        cursor += instruction.size
        encoding = getattr(instruction, "encoding", None)
        if encoding is None:
            continue
        branch = (instruction.group(CS_GRP_JUMP) or instruction.group(CS_GRP_CALL))
        if getattr(encoding, "disp_size", 0) == 4:
            fields[instruction.address + encoding.disp_offset] = {
                "kind": "DIR32", "width": 4, "branch": False,
                "insn_offset": instruction.address, "insn_size": instruction.size,
                "mnemonic": instruction.mnemonic}
        if getattr(encoding, "imm_size", 0) == 4:
            fields[instruction.address + encoding.imm_offset] = {
                "kind": "REL32" if branch else "DIR32", "width": 4,
                "branch": bool(branch), "insn_offset": instruction.address,
                "insn_size": instruction.size, "mnemonic": instruction.mnemonic}
    _FIELD_CACHE[key] = fields
    return fields


def _operand_form(code, offset, reloc_type):
    """Classify a relocation field against the instruction that encodes it."""
    if offset < 1 or offset + 4 > len(code):
        return None, "relocation operand is outside function bytes"
    if reloc_type not in (IMAGE_REL_I386_REL32, IMAGE_REL_I386_DIR32):
        return None, "unsupported relocation type 0x%04x" % reloc_type
    fields = _decode_fields(code)
    if fields is None:
        return None, "x86 decoding is unavailable"
    field = fields.get(offset)
    if field is None:
        return None, "no decoded 4-byte operand field starts at this relocation"
    if reloc_type == IMAGE_REL_I386_REL32 and not field["branch"]:
        return None, "REL32 relocation does not land on a branch displacement"
    if reloc_type == IMAGE_REL_I386_DIR32 and field["branch"]:
        return None, "DIR32 relocation lands on a branch displacement"
    kind = "REL32" if reloc_type == IMAGE_REL_I386_REL32 else "DIR32"
    return {"kind": kind, "operand_offset": offset, "width": 4,
            "insn_offset": field["insn_offset"], "insn_size": field["insn_size"],
            "mnemonic": field["mnemonic"]}, None


def _raw_target(reference, address, form):
    off = form["operand_offset"]
    if form["kind"] == "REL32":
        return address + off + 4 + struct.unpack_from("<i", reference, off)[0]
    return struct.unpack_from("<I", reference, off)[0]


def _reference_relocations(reference):
    """Return relocation-shaped operands decoded from raw-XBE code.

    The raw XBE has no relocation table, so these are the sites a relocation
    could plausibly occupy.  They are evidence about the reference's shape, not
    ground truth about the candidate's, and the comparison never lets a
    disagreement here veto a function.
    """
    fields = _decode_fields(reference)
    if fields is None:
        return None
    sites = []
    for offset in sorted(fields):
        field = fields[offset]
        reloc_type = IMAGE_REL_I386_REL32 if field["branch"] else IMAGE_REL_I386_DIR32
        sites.append({"offset": offset, "type": reloc_type,
                      "type_name": "REL32" if field["branch"] else "DIR32"})
    return sites


def _identity_evidence(relocation, form, candidate, reference, address, target_identities,
                       candidate_form=None):
    """Return optional target evidence; never gate structural byte matching."""
    target = _raw_target(reference, address, form)
    candidate_off = (candidate_form or form)["operand_offset"]
    addend = (struct.unpack_from("<I", candidate, candidate_off)[0]
              if form["kind"] == "DIR32" else
              struct.unpack_from("<i", candidate, candidate_off)[0])
    symbol = relocation.get("symbol") or {}
    symbol_name = symbol.get("name")
    if not symbol_name:
        identity = {"status": "unresolved", "reason": "relocation has no symbol base address"}
    else:
        normalized = _normal_name(symbol_name)
        match = re.fullmatch(r"FUN_([0-9a-fA-F]{8})", normalized or "")
        if match:
            symbol_base = int(match.group(1), 16)
        else:
            addresses = _raw_addresses_for_name(symbol_name)
            symbol_base = addresses[0] if len(addresses) == 1 else None
        if symbol_base is None:
            identity = {"status": "unresolved", "reason": "relocation symbol has no unique raw-XBE base address"}
        else:
            logical_target = symbol_base + addend
            convention = ("S+A (REL32 field is linked as S+A-P; logical target is S+A)"
                          if form["kind"] == "REL32" else "S+A (DIR32)")
            identity = _target_identity(symbol, target, logical_target, convention,
                                        (target_identities or {}).get(target))
    return {"raw_target": "0x%08x" % target, "candidate_addend": addend,
            "evidence": identity}


_INSTRUCTION_CACHE = {}


def _decode_instructions(code):
    """Decode a complete byte string into offset/size/mnemonic records."""
    key = bytes(code)
    if key in _INSTRUCTION_CACHE:
        return _INSTRUCTION_CACHE[key]
    try:
        from capstone import CS_ARCH_X86, CS_MODE_32, Cs
    except ImportError:
        return None
    decoder = Cs(CS_ARCH_X86, CS_MODE_32)
    records = []
    cursor = 0
    for instruction in decoder.disasm(key, 0):
        if instruction.address != cursor:
            return None
        records.append({"offset": instruction.address, "size": instruction.size,
                        "mnemonic": instruction.mnemonic,
                        "bytes": bytes(instruction.bytes)})
        cursor += instruction.size
    if cursor != len(key):
        return None
    _INSTRUCTION_CACHE[key] = records
    return records


def _instruction_alignment(candidate_insns, reference_insns):
    """Return a deterministic global instruction alignment.

    Mnemonics are the primary correspondence signal, while exact bytes, size,
    and opcode bytes break ties inside repeated ``push``/``mov``/branch runs.
    A compact backpointer matrix bounds memory. Very large products retain the
    prior SequenceMatcher fallback and are identified in the result metadata.
    """
    cell_cap = 4_000_000
    candidate_count = len(candidate_insns)
    reference_count = len(reference_insns)
    if candidate_count * reference_count > cell_cap:
        candidate_mnemonics = [item["mnemonic"] for item in candidate_insns]
        reference_mnemonics = [item["mnemonic"] for item in reference_insns]
        matcher = SequenceMatcher(None, candidate_mnemonics, reference_mnemonics,
                                  autojunk=False)
        pairs = []
        for tag, c1, c2, r1, r2 in matcher.get_opcodes():
            if tag == "equal":
                pairs.extend((candidate_insns[c1 + index], reference_insns[r1 + index])
                             for index in range(c2 - c1))
                continue
            if tag == "replace":
                shared = min(c2 - c1, r2 - r1)
                pairs.extend((candidate_insns[c1 + index], reference_insns[r1 + index])
                             for index in range(shared))
                pairs.extend((candidate_insns[index], None)
                             for index in range(c1 + shared, c2))
                pairs.extend((None, reference_insns[index])
                             for index in range(r1 + shared, r2))
                continue
            if tag == "delete":
                pairs.extend((candidate_insns[index], None) for index in range(c1, c2))
            elif tag == "insert":
                pairs.extend((None, reference_insns[index]) for index in range(r1, r2))
        return pairs, "mnemonic_sequence_matcher_fallback_v1", None

    gap_penalty = -4
    previous = [gap_penalty * index for index in range(reference_count + 1)]
    backpointers = [bytearray(reference_count + 1)
                    for _ in range(candidate_count + 1)]
    for index in range(1, candidate_count + 1):
        backpointers[index][0] = 2
    for index in range(1, reference_count + 1):
        backpointers[0][index] = 3

    for candidate_index in range(1, candidate_count + 1):
        candidate_insn = candidate_insns[candidate_index - 1]
        current = [gap_penalty * candidate_index] + [0] * reference_count
        for reference_index in range(1, reference_count + 1):
            reference_insn = reference_insns[reference_index - 1]
            if candidate_insn["mnemonic"] == reference_insn["mnemonic"]:
                pair_score = 8
                if candidate_insn["bytes"] == reference_insn["bytes"]:
                    pair_score += 4
                if candidate_insn["size"] == reference_insn["size"]:
                    pair_score += 1
                if (candidate_insn["bytes"] and reference_insn["bytes"] and
                        candidate_insn["bytes"][0] == reference_insn["bytes"][0]):
                    pair_score += 1
            else:
                pair_score = -6
            diagonal = previous[reference_index - 1] + pair_score
            up = previous[reference_index] + gap_penalty
            left = current[reference_index - 1] + gap_penalty
            best = max(diagonal, up, left)
            tied = int((diagonal == best) + (up == best) + (left == best) > 1)
            if diagonal == best:
                direction = 1
            elif up == best:
                direction = 2
            else:
                direction = 3
            backpointers[candidate_index][reference_index] = direction | (tied << 2)
            current[reference_index] = best
        previous = current

    pairs = []
    candidate_index = candidate_count
    reference_index = reference_count
    ambiguous_steps = 0
    while candidate_index or reference_index:
        encoded = backpointers[candidate_index][reference_index]
        ambiguous_steps += int(bool(encoded & 4))
        direction = encoded & 3
        if direction == 1:
            pairs.append((candidate_insns[candidate_index - 1],
                          reference_insns[reference_index - 1]))
            candidate_index -= 1
            reference_index -= 1
        elif direction == 2:
            pairs.append((candidate_insns[candidate_index - 1], None))
            candidate_index -= 1
        else:
            pairs.append((None, reference_insns[reference_index - 1]))
            reference_index -= 1
    pairs.reverse()
    return pairs, "weighted_global_dp_v1", ambiguous_steps


def aligned_byte_compare(candidate, relocations, reference, address,
                         target_identities=None):
    """Compare bytes within mnemonic-aligned instructions.

    Unlike the legacy positional percentage, this comparison resynchronizes at
    the next aligned instruction after an instruction grows, shrinks, appears,
    or disappears. Link-dependent operand bytes are excluded only when the
    candidate relocation resolves to the aligned raw-XBE operand's target.
    """
    candidate_insns = _decode_instructions(candidate)
    reference_insns = _decode_instructions(reference)
    if candidate_insns is None or reference_insns is None:
        return {"status": "unavailable", "reason": "complete x86 decoding failed"}

    candidate_relocations = {}
    malformed_relocations = 0
    for relocation in relocations:
        form, _reason = _operand_form(candidate, relocation["offset"], relocation["type"])
        if form is None:
            malformed_relocations += 1
            continue
        candidate_relocations.setdefault(form["insn_offset"], []).append((relocation, form))

    reference_fields = _decode_fields(reference)
    if reference_fields is None:
        return {"status": "unavailable", "reason": "reference operand decoding failed"}

    matching = 0
    compared = 0
    masked = 0
    normalized_exact_instructions = 0
    aligned_instructions = 0
    candidate_only = 0
    reference_only = 0
    resolved_relocations = 0
    unresolved_relocations = malformed_relocations
    uncertain_relocations = 0
    uncertain_relocation_bytes = 0
    unpaired_relocations = malformed_relocations
    first_difference = None

    alignment, alignment_method, ambiguous_steps = _instruction_alignment(
        candidate_insns, reference_insns)
    for candidate_insn, reference_insn in alignment:
        if candidate_insn is None or reference_insn is None:
            item = candidate_insn or reference_insn
            compared += item["size"]
            if candidate_insn is None:
                reference_only += 1
            else:
                candidate_only += 1
                unpaired = len(candidate_relocations.get(candidate_insn["offset"], []))
                unresolved_relocations += unpaired
                unpaired_relocations += unpaired
            if first_difference is None:
                first_difference = {
                    "candidate_offset": (candidate_insn or {}).get("offset"),
                    "reference_offset": (reference_insn or {}).get("offset"),
                    "candidate_mnemonic": (candidate_insn or {}).get("mnemonic"),
                    "reference_mnemonic": (reference_insn or {}).get("mnemonic"),
                    "kind": "unmatched_instruction"}
            continue

        aligned_instructions += 1
        candidate_mask = set()
        reference_mask = set()
        for relocation, candidate_form in candidate_relocations.get(
                candidate_insn["offset"], []):
            candidates = []
            for field_offset, field in reference_fields.items():
                if (field["insn_offset"] == reference_insn["offset"] and
                        field["kind"] == candidate_form["kind"]):
                    candidates.append((field_offset, field))
            if len(candidates) != 1:
                unresolved_relocations += 1
                unpaired_relocations += 1
                continue
            field_offset, field = candidates[0]
            reference_form = {
                "kind": field["kind"], "operand_offset": field_offset,
                "width": field["width"], "insn_offset": field["insn_offset"],
                "insn_size": field["insn_size"], "mnemonic": field["mnemonic"]}
            identity = _identity_evidence(
                relocation, reference_form, candidate, reference, address,
                target_identities, candidate_form=candidate_form)
            if identity["evidence"].get("status") != "resolved":
                unresolved_relocations += 1
                uncertain_relocations += 1
                uncertain_relocation_bytes += 4
                candidate_local = candidate_form["operand_offset"] - candidate_insn["offset"]
                reference_local = reference_form["operand_offset"] - reference_insn["offset"]
                candidate_mask.update(range(candidate_local, candidate_local + 4))
                reference_mask.update(range(reference_local, reference_local + 4))
                continue
            candidate_local = candidate_form["operand_offset"] - candidate_insn["offset"]
            reference_local = reference_form["operand_offset"] - reference_insn["offset"]
            candidate_mask.update(range(candidate_local, candidate_local + 4))
            reference_mask.update(range(reference_local, reference_local + 4))
            masked += 4
            resolved_relocations += 1

        candidate_bytes = [value for index, value in enumerate(candidate_insn["bytes"])
                           if index not in candidate_mask]
        reference_bytes = [value for index, value in enumerate(reference_insn["bytes"])
                           if index not in reference_mask]
        instruction_matching = 0
        instruction_compared = 0
        width = max(len(candidate_bytes), len(reference_bytes))
        for index in range(width):
            instruction_compared += 1
            if (index < len(candidate_bytes) and index < len(reference_bytes) and
                    candidate_bytes[index] == reference_bytes[index]):
                instruction_matching += 1
        matching += instruction_matching
        compared += instruction_compared
        if (len(candidate_bytes) == len(reference_bytes) and
                instruction_matching == instruction_compared):
            normalized_exact_instructions += 1
        elif first_difference is None:
            first_difference = {
                "candidate_offset": candidate_insn["offset"],
                "reference_offset": reference_insn["offset"],
                "candidate_mnemonic": candidate_insn["mnemonic"],
                "reference_mnemonic": reference_insn["mnemonic"],
                "kind": "instruction_bytes"}

    total_compared = compared + uncertain_relocation_bytes
    lower_accuracy = matching / total_compared if total_compared else None
    upper_accuracy = ((matching + uncertain_relocation_bytes) / total_compared
                      if total_compared else None)
    return {
        "status": "scored", "method": alignment_method,
        "byte_accuracy": lower_accuracy,
        "byte_accuracy_upper_bound": upper_accuracy,
        "matching_bytes": matching, "compared_bytes": total_compared,
        "stable_compared_bytes": compared,
        "masked_relocation_bytes": masked,
        "uncertain_relocation_bytes": uncertain_relocation_bytes,
        "aligned_instruction_pairs": aligned_instructions,
        "normalized_exact_instructions": normalized_exact_instructions,
        "candidate_only_instructions": candidate_only,
        "reference_only_instructions": reference_only,
        "resolved_relocations": resolved_relocations,
        "unresolved_relocations": unresolved_relocations,
        "uncertain_relocations": uncertain_relocations,
        "unpaired_relocations": unpaired_relocations,
        "accuracy_is_lower_bound": bool(uncertain_relocation_bytes),
        "accuracy_is_provisional": bool(unpaired_relocations),
        "alignment_ambiguous_steps": ambiguous_steps,
        "first_difference": first_difference,
        "confidence": "provisional"}


def compare(candidate, relocations, reference, address, target_identities=None):
    """Compare literal bytes after raw-XBE-approved relocation masking.

    The candidate relocation location/type sequence must match the conservative
    raw-XBE operand classification. Resolved target identity is recorded as
    evidence only and is deliberately not required for a structural exact.
    """
    result = {
        "verdict": "not comparable", "confidence": "low", "confidence_reasons": [],
        "candidate_length": len(candidate), "reference_length": len(reference),
        "candidate_relocation_count": len(relocations), "relocations": [],
        "matching_non_relocation_bytes": 0, "matching_bytes": 0, "masked_bytes": 0,
        "byte_accuracy": None,
        "byte_counts": {"candidate": len(candidate), "reference": len(reference),
                        "relocation_operand": 0, "non_relocation": 0,
                        "matching_non_relocation": 0, "mismatching_non_relocation": 0},
    }
    result["aligned_byte_match"] = aligned_byte_compare(
        candidate, relocations, reference, address, target_identities)
    candidate_mask = set()
    reference_mask = set()
    reference_sites = _reference_relocations(reference)
    if reference_sites is None:
        result["relocation_shape_evidence"] = {"status": "unavailable", "matched": False,
                                                "candidate": [], "reference": None,
                                                "identity_required": False}
        if relocations:
            result["confidence_reasons"].append("raw-XBE relocation classification is unavailable")
            return result
        reference_sites = []
        result["relocation_shape_evidence"]["status"] = "not needed"
    else:
        candidate_shape = [(item["offset"], item["type"]) for item in relocations]
        reference_shape = [(item["offset"], item["type"]) for item in reference_sites]
        matched = candidate_shape == reference_shape
        result["relocation_shape_evidence"] = {
            "status": "matched" if matched else "mismatch", "matched": matched,
            "identity_required": False,
            "candidate": [{"offset": item["offset"], "type": item["type"],
                           "type_name": item["type_name"]} for item in relocations],
            "reference": [{"offset": item["offset"], "type": item["type"],
                           "type_name": item["type_name"]} for item in reference_sites],
        }
        if not matched:
            # The candidate's COFF relocation table is exact metadata; the raw-XBE
            # side is a heuristic sweep that can over- or under-detect.  A sweep
            # disagreement is therefore not evidence of a candidate defect, so it
            # must not veto the whole function.  Masking stays candidate-driven:
            # every masked field is still required, below, to decode as the same
            # operand kind on both sides.  Reference-only detections are simply
            # left unmasked and compared literally, which can only lower the
            # score, never inflate it.
            result["confidence"] = "medium"
            result["confidence_reasons"].append(
                "raw-XBE relocation classification differs from the candidate table; "
                "masking is candidate-driven and reference-only sites are compared literally")

    identity_records = []
    for relocation in relocations:
        item = {"offset": relocation["offset"], "type": relocation["type"],
                "type_name": relocation["type_name"],
                "symbol": (relocation.get("symbol") or {}).get("name"),
                "comparable": False}
        form, reason = _operand_form(candidate, relocation["offset"], relocation["type"])
        if form is None:
            item["reason"] = reason
            result["relocations"].append(item)
            result["confidence_reasons"].append(reason)
            continue
        raw_form, raw_reason = _operand_form(reference, relocation["offset"], relocation["type"])
        if raw_form is None or raw_form["kind"] != form["kind"]:
            item["reason"] = raw_reason or "raw reference operand form differs"
            result["relocations"].append(item)
            result["confidence_reasons"].append(item["reason"])
            continue
        identity = _identity_evidence(relocation, raw_form, candidate, reference, address,
                                      target_identities, candidate_form=form)
        item.update({"operand_kind": form["kind"], "raw_target": identity["raw_target"],
                     "target_identity": identity["evidence"], "comparable": True,
                     "reason": "location/type and operand encoding matched; identity is evidence only"})
        result["relocations"].append(item)
        identity_records.append(identity["evidence"])
        candidate_mask.update(range(form["operand_offset"], form["operand_offset"] + 4))
        reference_mask.update(range(raw_form["operand_offset"], raw_form["operand_offset"] + 4))

    result["identity_evidence"] = {
        "required_for_structural_match": False, "records": identity_records,
        "resolved": sum(1 for item in identity_records if item.get("status") == "resolved"),
        "unresolved": sum(1 for item in identity_records if item.get("status") != "resolved"),
        "status": ("resolved" if identity_records and
                   all(item.get("status") == "resolved" for item in identity_records)
                   else "unresolved" if identity_records and
                   all(item.get("status") != "resolved" for item in identity_records)
                   else "partial" if identity_records else "not present"),
    }
    # An unmaskable relocation is not a reason to discard the function.  Masking
    # can only ever raise a score, so declining to mask is the conservative
    # direction: those 4 bytes are compared literally instead.  The candidate
    # holds a placeholder where the linked reference holds a real address, so
    # they will usually count as mismatches and the reported accuracy becomes a
    # lower bound.  That is reported, not hidden.
    unmasked = [item for item in result["relocations"] if not item["comparable"]]
    if unmasked:
        result["unmasked_relocations"] = len(unmasked)
        result["accuracy_is_lower_bound"] = True
        result["confidence"] = "medium"
        result["confidence_reasons"].append(
            "%d relocation field(s) could not be justified for masking and were compared "
            "literally; the reported accuracy is a lower bound" % len(unmasked))
    else:
        result["unmasked_relocations"] = 0
        result["accuracy_is_lower_bound"] = False
    if candidate_mask != reference_mask:
        result["confidence_reasons"].append("candidate and raw operand masks differ")
        return result

    result["byte_counts"]["relocation_operand"] = len(candidate_mask)
    result["byte_counts"]["unmasked_relocation_operand"] = 4 * len(unmasked)
    result["masked_bytes"] = len(candidate_mask)
    overlap = min(len(candidate), len(reference))
    compared = [(i, candidate[i], reference[i]) for i in range(overlap)
                if i not in candidate_mask and i not in reference_mask]
    matching = sum(1 for _i, left, right in compared if left == right)
    # A size mismatch must not be scored on the overlapping prefix alone: a
    # truncated candidate whose prefix matched would otherwise read as 100%.
    # Every byte only one side has counts against the accuracy.
    longer_mask = candidate_mask if len(candidate) >= len(reference) else reference_mask
    tail = sum(1 for i in range(overlap, max(len(candidate), len(reference)))
               if i not in longer_mask)
    denominator = len(compared) + tail
    result["matching_non_relocation_bytes"] = matching
    result["matching_bytes"] = matching
    result["byte_counts"].update({"non_relocation": denominator,
                                  "matching_non_relocation": matching,
                                  "mismatching_non_relocation": denominator - matching,
                                  "size_mismatch_bytes": tail})
    result["byte_accuracy"] = matching / denominator if denominator else None
    differing = next(((i, left, right) for i, left, right in compared if left != right), None)
    if len(candidate) != len(reference) or differing is not None:
        result["verdict"] = "structural differ"
        result["confidence"] = "high"
        if len(candidate) != len(reference):
            result["confidence_reasons"].append("function sizes differ")
        if differing:
            result["first_difference"] = differing[0]
        return result
    result["verdict"] = "structural exact"
    result["confidence"] = "high"
    result["confidence_reasons"].append(
        "all non-relocation bytes match; relocation identity was not required")
    return result


def audit(candidate_obj, function, address, source=None):
    """Run the relocation-shape-aware comparison against the pristine raw XBE."""
    candidate, relocs, provenance = _parse_coff(candidate_obj, function)
    reference, error = xref.function_bytes(address)
    record = {"schema_version": 1, "lane": "raw_xbe_structural",
              "function": function, "address": "0x%08x" % address,
              "candidate": {"path": str(candidate_obj), "sha256": _sha256(candidate), **provenance},
              # This is the pristine XBE FILE hash, the provenance the report
              # checks.  The hash of the function's own span is recorded
              # separately as sha256_span; confusing the two makes every record
              # fail validation.
              "reference": {"path": str(xref.XBE), "sha256": _hash_path(xref.XBE)}}
    if source is not None:
        record["source"] = {"path": str(source), "sha256": strict.sha256_file(source)}
    if reference is None:
        record.update({"verdict": "not comparable", "confidence": "low", "reason": error})
        return record
    # Register-argument functions ARE scored.  Their original receives arguments
    # in registers and our build reaches it through a generated thunk, so the
    # patched binary genuinely does not reproduce the original's bytes.  That is
    # a real fidelity gap, not a measurement artifact, and refusing to score it
    # would report our accuracy as higher than it is.  The flag records why the
    # number is low so it can be filtered and acted on.
    register_argument = address in _register_argument_addresses()
    if register_argument:
        record["register_argument"] = True
        record["register_argument_note"] = (
            "the original receives arguments in registers; our standalone compile carries a "
            "stack-loading prologue it does not have, so every later byte is shifted")
    extent = xref.function_extent(address)
    if extent is not None:
        end, kind, bound_provenance = extent
        record["reference"].update({"start": "0x%08x" % address, "end": "0x%08x" % end,
                                     "length": len(reference), "sha256_span": _sha256(reference),
                                     "bound_kind": kind, "bound_provenance": bound_provenance})
        record["bounds"] = {"start": "0x%08x" % address, "end": "0x%08x" % end,
                             "sha256": _hash_path(ROOT / "tools" / "verify" / "function_bounds.json")}
    record.update(compare(candidate, relocs, reference, address))
    return record


def _write_record(record, output):
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_name(output.name + ".tmp")
    temporary.write_text(json.dumps(record, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    os.replace(temporary, output)


def _hash_path(path):
    return strict.sha256_file(path) if path.is_file() else None


_REG_ARG_CACHE = {}


def _register_argument_addresses():
    """Addresses whose original takes arguments in registers (kb.json @<reg>).

    The build system reaches these through a generated thunk, so the original
    has no prologue loading arguments off the stack.  A standalone compile of
    our C does, which shifts every later byte: the comparison is measuring a
    calling convention, not lift quality.  These are reported as not comparable
    rather than scored, because a 6% here means "harness cannot see this", not
    "the lift is wrong".
    """
    if _REG_ARG_CACHE:
        return _REG_ARG_CACHE["addresses"]
    addresses = set()
    try:
        kb = json.loads((ROOT / "kb.json").read_text(encoding="utf-8"))
    except (OSError, ValueError):
        _REG_ARG_CACHE["addresses"] = addresses
        return addresses
    entries = []
    if isinstance(kb, dict):
        entries.extend(kb.values())
        for obj in kb.get("objects", []):
            if isinstance(obj, dict):
                entries.extend(obj.get("functions", []))
    for entry in entries:
        if not isinstance(entry, dict) or "@<" not in (entry.get("decl") or ""):
            continue
        addr = entry.get("addr")
        if not addr:
            continue
        try:
            addresses.add(int(addr, 0))
        except (TypeError, ValueError):
            continue
    _REG_ARG_CACHE["addresses"] = addresses
    return addresses


def _vc71_sources():
    """Map address -> source path using the VC71 score snapshot.

    Only a small minority of kb.json function entries carry ``source_path``.
    The VC71 snapshot records the source it compiled for every function it
    scored, so it resolves the rest without guessing.  Missing or unreadable
    snapshot means no fallback, never an invented path.
    """
    snapshot = ROOT / "tools" / "verify" / "vc71_current.json"
    if not snapshot.is_file():
        return {}
    try:
        scores = json.loads(snapshot.read_text(encoding="utf-8")).get("scores", {})
    except (ValueError, OSError):
        return {}
    sources = {}
    for name, record in scores.items():
        if not isinstance(record, dict):
            continue
        addr = record.get("addr")
        source = record.get("source")
        if not addr or not source:
            continue
        try:
            sources[int(addr, 0)] = (source, name)
        except (TypeError, ValueError):
            continue
    return sources


def _eligible_functions(source_filter=None):
    kb = json.loads((ROOT / "kb.json").read_text(encoding="utf-8"))
    vc71_sources = _vc71_sources()
    if isinstance(source_filter, str):
        source_filter = {Path(source_filter).as_posix()}
    elif source_filter:
        source_filter = {Path(path).as_posix() for path in source_filter}
    entries = list(kb.values()) if isinstance(kb, dict) else list(kb)
    if isinstance(kb, dict):
        for obj in kb.get("objects", []):
            entries.extend(obj.get("functions", []))
    eligible = {}
    for entry in entries:
        if not isinstance(entry, dict) or entry.get("ported") is not True:
            continue
        if not entry.get("addr"):
            continue
        address = int(entry["addr"], 0)
        vc71_source, vc71_name = vc71_sources.get(address, (None, None))
        source_path = entry.get("source_path") or entry.get("source") or vc71_source
        if not source_path:
            continue
        source_path = source_path if source_path.startswith("src/") else "src/halo/" + source_path
        if source_filter and Path(source_path).as_posix() not in source_filter:
            continue
        source = ROOT / source_path
        if not source.is_file():
            continue
        # kb.json often omits the name; the VC71 snapshot records the symbol
        # actually compiled, which is what the candidate COFF exports.  The
        # synthesized fallback is lowercase to match the repository convention.
        eligible[address] = {"function": entry.get("name") or vc71_name or "FUN_%08x" % address,
                             "address": address, "source": source}
    return list(eligible.values())


def _bounds_hash():
    return _hash_path(ROOT / "tools" / "verify" / "function_bounds.json")


def _decl_hash():
    return _hash_path(ROOT / "build" / "generated" / "decl.h")


def _compiler_token():
    return getattr(vc71, "VC71_CL_WSL", "VC71") + " /O2 /Oy- /GF /Gy /Gd /W0 /Zl /X"


def _record_error(item, reason, source_sha256=None):
    return {"function": item["function"], "address": "0x%08x" % item["address"],
            "verdict": "not comparable", "confidence": "low", "reason": reason,
            "source": {"path": str(item["source"]), "sha256": source_sha256 or _hash_path(item["source"])},
            "reference": {"path": str(xref.XBE), "sha256": _hash_path(xref.XBE)}}


def _audit_tu(source, items, artifact_dir, decl_hash):
    source_sha256 = _hash_path(source)
    candidate = artifact_dir / ("tu-%s.obj" % hashlib.sha256(str(source).encode()).hexdigest()[:16])
    if not vc71.compile_vc71(source, candidate):
        return [(item, _record_error(item, "VC71 compilation failed", source_sha256)) for item in items]
    records = []
    for item in items:
        try:
            record = audit(candidate, item["function"], item["address"], source)
        except (OSError, strict.NotComparable) as exc:
            record = _record_error(item, str(exc), source_sha256)
        record["generated_at"] = datetime.now(timezone.utc).isoformat()
        record["tool"] = {"version": "1", "compiler": _compiler_token(), "decl_sha256": decl_hash,
                          "bounds_sha256": _bounds_hash(), "reference_authority": "pristine raw XBE + bounds"}
        records.append((item, record))
    return records


def _write_summary(records, invocation, source_count, compile_failures, eligible=None):
    def empty():
        return {"functions": 0, "original_bytes": 0, "reference_bytes": 0,
                "matching_non_relocation_bytes": 0, "compared_non_relocation_bytes": 0,
                "masked_relocation_bytes": 0}
    totals = {verdict: empty() for verdict in ("structural exact", "structural differ", "not comparable", "not audited")}
    for record in records:
        verdict = record.get("verdict", "not audited")
        totals.setdefault(verdict, empty())
        row = totals[verdict]
        row["functions"] += 1
        reference_bytes = record.get("reference", {}).get("length", 0) or 0
        row["original_bytes"] += reference_bytes
        row["reference_bytes"] += reference_bytes
        row["matching_non_relocation_bytes"] += record.get("matching_non_relocation_bytes", 0) or 0
        row["compared_non_relocation_bytes"] += record.get("byte_counts", {}).get("non_relocation", 0) or 0
        row["masked_relocation_bytes"] += record.get("byte_counts", {}).get("relocation_operand", 0) or 0
    audited_keys = {(record.get("address"), record.get("function")) for record in records}
    for item in eligible or []:
        key = ("0x%08x" % item["address"], item["function"])
        if key not in audited_keys:
            totals["not audited"]["functions"] += 1
            extent = xref.function_extent(item["address"])
            if extent is not None:
                totals["not audited"]["original_bytes"] += extent[0] - item["address"]
                totals["not audited"]["reference_bytes"] += extent[0] - item["address"]
    comparable = totals["structural exact"]["functions"] + totals["structural differ"]["functions"]
    comparable_bytes = totals["structural exact"]["reference_bytes"] + totals["structural differ"]["reference_bytes"]
    matching = sum(row["matching_non_relocation_bytes"] for row in totals.values())
    compared = sum(row["compared_non_relocation_bytes"] for row in totals.values())
    exact = totals["structural exact"]["functions"]
    aligned = [record.get("aligned_byte_match", {}) for record in records]
    aligned = [item for item in aligned if item.get("status") == "scored"]
    aligned_matching = sum(item.get("matching_bytes", 0) or 0 for item in aligned)
    aligned_compared = sum(item.get("compared_bytes", 0) or 0 for item in aligned)
    aligned_uncertain = sum(item.get("uncertain_relocation_bytes", 0) or 0 for item in aligned)
    summary = {"schema_version": 1, "lane": "raw_xbe_structural", "tool_version": "1", "invocation": invocation,
               "generated_at": datetime.now(timezone.utc).isoformat(),
               "xbe_sha256": _hash_path(xref.XBE), "bounds_sha256": _bounds_hash(), "decl_sha256": _decl_hash(),
               "reference_authority": "pristine raw XBE", "source_tu_count": source_count,
               "candidate_compile_failures": compile_failures, "totals": totals,
               "function_totals": {"structural_exact": exact, "structural_differ": totals["structural differ"]["functions"],
                                   "not_comparable": totals["not comparable"]["functions"], "not_audited": totals["not audited"]["functions"]},
               "byte_totals": {"reference": sum(row["reference_bytes"] for row in totals.values()),
                               "structural_exact": totals["structural exact"]["reference_bytes"],
                               "comparable": comparable_bytes, "matching_non_relocation": matching,
                               "matching_bytes": matching, "masked_bytes": sum(row["masked_relocation_bytes"] for row in totals.values()),
                               "compared_non_relocation": compared,
                               "byte_accuracy": matching / compared if compared else None,
                               "masked_accuracy": (matching + sum(row["masked_relocation_bytes"] for row in totals.values())) / sum(row["reference_bytes"] for row in totals.values()) if sum(row["reference_bytes"] for row in totals.values()) else None},
               "aligned_byte_totals": {
                   "scored_functions": len(aligned),
                   "matching_bytes": aligned_matching,
                   "compared_bytes": aligned_compared,
                   "uncertain_relocation_bytes": aligned_uncertain,
                   "byte_accuracy_lower": aligned_matching / aligned_compared if aligned_compared else None,
                   "byte_accuracy_upper": ((aligned_matching + aligned_uncertain) / aligned_compared
                                           if aligned_compared else None),
                   "provisional_functions": sum(
                       1 for item in aligned if item.get("accuracy_is_provisional"))},
               "exact_among_comparable": exact / comparable if comparable else None,
               "exact_byte_coverage_of_comparable": totals["structural exact"]["reference_bytes"] / comparable_bytes if comparable_bytes else None}
    _write_record(summary, ROOT / "artifacts" / "raw_xbe_structural" / "summary.json")
    return summary


def _run_single(args):
    artifact_dir = ROOT / "artifacts" / "raw_xbe_structural"
    output = args.output or artifact_dir / ("%08x-%s.json" % (args.address, args.function))
    try:
        if args.candidate is not None:
            record = audit(args.candidate, args.function, args.address)
        else:
            candidate = artifact_dir / ("%08x-%s.obj" % (args.address, args.function))
            vc71.regen_decl_header(quiet=True)
            if not vc71.compile_vc71(args.source.resolve(), candidate):
                return 2
            record = audit(candidate, args.function, args.address, args.source)
    except (OSError, strict.NotComparable) as exc:
        source = args.source if args.source is not None else args.candidate
        record = {"function": args.function, "address": "0x%08x" % args.address,
                  "verdict": "not comparable", "confidence": "low", "reason": str(exc),
                  "candidate": {"path": str(source)}}
    _write_record(record, output)
    aligned = record.get("aligned_byte_match", {})
    aligned_suffix = ""
    if aligned.get("status") == "scored" and aligned.get("byte_accuracy") is not None:
        upper = aligned.get("byte_accuracy_upper_bound", aligned["byte_accuracy"])
        if upper != aligned["byte_accuracy"]:
            accuracy_text = "%.1f-%.1f%%" % (
                100.0 * aligned["byte_accuracy"], 100.0 * upper)
        else:
            accuracy_text = "%.1f%%" % (100.0 * aligned["byte_accuracy"])
        aligned_suffix = ", aligned bytes %s (%d/%d), normalized-exact insns %d/%d%s" % (
            accuracy_text, aligned["matching_bytes"], aligned["compared_bytes"],
            aligned["normalized_exact_instructions"], aligned["aligned_instruction_pairs"],
            "?" if aligned.get("accuracy_is_provisional") else "")
    print("%s: %s%s (%s)" % (
        record.get("function", args.function), record["verdict"], aligned_suffix, output))
    return {"structural exact": 0, "structural differ": 1, "not comparable": 2}.get(record["verdict"], 2)


def _run_populate(args):
    items = _eligible_functions(args.source)
    grouped = {}
    for item in items:
        grouped.setdefault(item["source"], []).append(item)
    if not vc71.regen_decl_header(quiet=True):
        raise SystemExit("could not pin decl.h")
    artifact_dir = ROOT / "artifacts" / "raw_xbe_structural"
    artifact_dir.mkdir(parents=True, exist_ok=True)
    records = []
    compile_failures = 0
    groups = sorted(grouped.items(), key=lambda pair: str(pair[0]))
    with concurrent.futures.ProcessPoolExecutor(max_workers=max(1, min(args.workers, len(groups) or 1))) as executor:
        futures = [executor.submit(_audit_tu, source, source_items, artifact_dir, _decl_hash()) for source, source_items in groups]
        for future in futures:
            results = future.result()
            if results and all(record.get("reason") == "VC71 compilation failed" for _, record in results):
                compile_failures += 1
            for item, record in results:
                _write_record(record, artifact_dir / ("%08x-%s.json" % (item["address"], item["function"])))
                records.append(record)
    summary = _write_summary(records, sys.argv[1:], len(grouped), compile_failures, items)
    print(json.dumps(summary["totals"], sort_keys=True))
    return 0 if not compile_failures else 2


def main(argv=None):
    argv = list(sys.argv[1:] if argv is None else argv)
    if argv and argv[0] == "single":
        argv.pop(0)
    if argv and argv[0] not in ("populate", "check", "show"):
        parser = argparse.ArgumentParser(description=__doc__)
        parser.add_argument("source", type=Path, nargs="?")
        parser.add_argument("--object", type=Path, dest="candidate")
        parser.add_argument("--function", required=True)
        parser.add_argument("--address", required=True, type=lambda value: int(value, 0))
        parser.add_argument("--output", type=Path)
        parsed = parser.parse_args(argv)
        if parsed.source is None and parsed.candidate is None:
            parser.error("provide a source or --object")
        if parsed.source is not None and parsed.candidate is not None:
            parser.error("source and --object are mutually exclusive")
        return _run_single(parsed)
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    populate = subparsers.add_parser("populate")
    populate.add_argument("--source")
    populate.add_argument("--workers", type=int, default=1)
    check = subparsers.add_parser("check")
    check.add_argument("--changed", action="store_true")
    subparsers.add_parser("show")
    args = parser.parse_args(argv)
    if args.command == "populate":
        return _run_populate(args)
    summary_path = ROOT / "artifacts" / "raw_xbe_structural" / "summary.json"
    if not summary_path.is_file():
        print("no raw-XBE structural summary")
        return 2
    summary = json.loads(summary_path.read_text(encoding="utf-8"))
    if args.command == "show":
        for verdict, totals in summary["totals"].items():
            print("%-20s %8d functions %10d bytes" % (verdict + ":", totals["functions"], totals["original_bytes"]))
        print("Byte accuracy (non-relocation): %s" % summary["byte_totals"]["byte_accuracy"])
        print("Exact among comparable functions: %s" % summary["exact_among_comparable"])
        print("Exact byte coverage of comparable: %s" % summary["exact_byte_coverage_of_comparable"])
        print("Relocation identity is evidence only; it is not required for structural exactness.")
        return 0
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
