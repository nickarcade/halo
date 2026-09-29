#!/usr/bin/env python3
"""Name evidence for a global address, read from the pristine 2276 XBE.

    name_evidence.py 0x5a8d58 [0x5a90bc ...]

For each address, reports:

* ``hs_global``: an hs-globals table entry ``{char *name, int16 type, ...,
  void *address}`` that points at it. The name is T1 evidence
  (``naming-confidence``), and the type is the hs type index.
* ``float`` / ``double``: the value, when the address holds initialized data.
  Use it to name ``.rdata`` pool constants.

It reports only what the binary states. An address with no row has no name
evidence here, so check assert strings (``recover_assert_sites.py``) and
kb.json before you give it a mechanical name.
"""

import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "equivalence"))
from xbe_image import load_xbe, read_va, section_at  # noqa: E402


def _cstring(raw, secs, va):
    sec = section_at(secs, va)
    if sec is None or not sec.raw_contains_va(va):
        return None
    data = read_va(raw, secs, va, 96)
    end = data.find(b"\0")
    text = data[:end if end >= 0 else len(data)]
    if len(text) < 3 or not all(0x20 < c < 0x7f for c in text):
        return None
    return text.decode("ascii")


def hs_globals(raw, secs, address):
    """hs-global entries whose address slot (entry+8) holds ``address``."""
    hits = []
    needle = struct.pack("<I", address)
    for sec in secs:
        if sec.name not in (".data", ".rdata", "DOLBY", "XON_RD") and not sec.name.startswith(".data"):
            continue
        blob = raw[sec.raw_off:sec.raw_off + sec.raw_size]
        index = blob.find(needle)
        while index != -1:
            if index >= 8 and index % 4 == 0:
                name_ptr, type_word = struct.unpack_from("<IH", blob, index - 8)
                name = _cstring(raw, secs, name_ptr)
                if name is not None:
                    hits.append({"entry": sec.va + index - 8, "name": name, "type": type_word})
            index = blob.find(needle, index + 1)
    return hits


def main(argv):
    if not argv:
        print(__doc__.strip())
        return 2
    raw, secs = load_xbe()
    for arg in argv:
        address = int(arg, 16)
        print("%#x" % address)
        for hit in hs_globals(raw, secs, address):
            print("  hs_global  %-40s type=%d  entry=%#x" % (hit["name"], hit["type"], hit["entry"]))
        sec = section_at(secs, address)
        if sec is not None and sec.raw_contains_va(address):
            value = read_va(raw, secs, address, 8)
            print("  float      %r" % struct.unpack_from("<f", value)[0])
            print("  double     %r" % struct.unpack_from("<d", value)[0])
        elif sec is None:
            print("  (not in any section)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
