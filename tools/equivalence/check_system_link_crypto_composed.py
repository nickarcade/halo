"""Compare active message wrappers with the original 2276 frame transform.

The candidate inlines TEA, so the generic differential intercepts the
original's helper calls. This runner lets only the original TEA/XOR helpers
execute within their committed function bounds, then compares both full
frames against the 2276-backed composition model.
"""

import contextlib
import io
import json
import struct
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, "tools/equivalence")
import unicorn_diff

MASK = 0xffffffff
KEY = bytes.fromhex("4030201080706050c0b0a09000f0e0d0")
KEY_WORDS = struct.unpack("<2I", KEY[:8]) * 2
BASE = bytes(range(1, 31))
old_run = unicorn_diff._run_function
states = []
NATIVE_CRYPTO = {
    0x807d0: 0x80817,  # key_message_xor_keystream
    0x80820: 0x808a8,  # tea_encrypt
    0x808b0: 0x8093f,  # tea_decrypt
}


def capture(*args, **kwargs):
    if kwargs.get("entry_va") is not None and not kwargs.get("lifted"):
        kwargs["intercept_vas"] = {
            address: sentinel for address, sentinel in kwargs["intercept_vas"].items()
            if address not in NATIVE_CRYPTO
        }
        kwargs["native_callee_ranges"] = tuple(kwargs.get("native_callee_ranges") or ()) + tuple(
            NATIVE_CRYPTO.items()
        )
    state = old_run(*args, **kwargs)
    states.append(state)
    return state


unicorn_diff._run_function = capture


def tea_encrypt(left, right):
    total = 0
    for _ in range(32):
        total = (total + 0x9e3779b9) & MASK
        mix = (((right >> 5) + KEY_WORDS[1]) ^
               (((right << 4) & MASK) + KEY_WORDS[0]) ^
               ((right + total) & MASK))
        left = (left + mix) & MASK
        mix = (((left >> 5) + KEY_WORDS[3]) ^
               (((left << 4) & MASK) + KEY_WORDS[2]) ^
               ((left + total) & MASK))
        right = (right + mix) & MASK
    return left, right


def tea_decrypt(left, right):
    total = 0xc6ef3720
    for _ in range(32):
        mix = (((left >> 5) + KEY_WORDS[3]) ^
               (((left << 4) & MASK) + KEY_WORDS[2]) ^
               ((left + total) & MASK))
        right = (right - mix) & MASK
        mix = (((right >> 5) + KEY_WORDS[1]) ^
               (((right << 4) & MASK) + KEY_WORDS[0]) ^
               ((right + total) & MASK))
        left = (left - mix) & MASK
        total = (total + 0x61c88647) & MASK
    return left, right


def expected_frame(frame, size, decrypt):
    output = bytearray(frame)
    for pos in range(2, size - 7, 8):
        left, right = struct.unpack_from("<2I", output, pos)
        left, right = (tea_decrypt if decrypt else tea_encrypt)(left, right)
        struct.pack_into("<2I", output, pos, left, right)
    remainder = (size - 2) % 8
    start = size - remainder
    for index in range(remainder):
        output[start + index] = (~(output[start + index] ^ KEY[index])) & 0xff
    header = int.from_bytes(output[:2], "little")
    header = (header & ~1) if decrypt else (header | 1)
    output[:2] = header.to_bytes(2, "little")
    return bytes(output)


with tempfile.TemporaryDirectory() as temporary:
    for size, other_flag in ((size, flag)
                             for size in (2, 3, 9, 10, 11, 17, 18, 19)
                             for flag in (0, 2)):
        plain = ((size << 4) | other_flag).to_bytes(2, "little") + BASE
        for decrypt in (False, True):
            incoming = expected_frame(plain, size, False) if decrypt else plain
            if decrypt and expected_frame(incoming, size, True) != plain:
                raise AssertionError((size, "reference round trip"))
            expect = plain if decrypt else expected_frame(plain, size, False)
            snapshot = Path(temporary) / f"crypto_{size}_{other_flag}_{int(decrypt)}.json"
            snapshot.write_text(json.dumps({
                "description": f"crypto size={size} flag={other_flag} decrypt={decrypt}",
                "build_label": "synthetic", "regions": {},
                "arg_overrides": {"msgptr": incoming.hex(), "key": KEY.hex()}
            }))
            states.clear()
            sys.argv = ["unicorn_diff.py", "message_decrypt" if decrypt else "message_encrypt",
                        "--allow-stubs", "--real-callees", "--oracle-native-callees",
                        "--state-snapshot", str(snapshot), "--seeds", "1",
                        "--no-concolic", "--no-leaf-cache", "-q"]
            with contextlib.redirect_stdout(io.StringIO()):
                try:
                    unicorn_diff.main()
                except SystemExit:
                    pass
            if len(states) != 2 or states[0].error or states[1].error:
                raise AssertionError((size, decrypt, len(states),
                                      [state.error for state in states]))
            expected_helpers = set()
            if (size - 2) // 8:
                expected_helpers.add(0x808b0 if decrypt else 0x80820)
            if (size - 2) % 8:
                expected_helpers.add(0x807d0)
            if not expected_helpers.issubset(states[0].visited_pcs):
                raise AssertionError((size, decrypt, "original helper not executed"))
            for side, state in (("original", states[0]), ("candidate", states[1])):
                actual = state.scratch_data[:len(incoming)]
                if actual != expect:
                    raise AssertionError((size, decrypt, side, actual.hex(), expect.hex()))
            print(size, other_flag, "decrypt" if decrypt else "encrypt", "matches")
