"""Check active 64-bit key math against the binary-backed PAL operations.

The generic oracle stubs same-object arithmetic callees while Clang executes
them. This checks the complete candidate result for concrete nontrivial
values. The 2276 divide intentionally computes denominator / numerator.
These are composition checks, not same-context direct oracle verdicts.
"""

import contextlib
import io
import json
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, "tools/equivalence")
import unicorn_diff

MASK64 = (1 << 64) - 1
old_run = unicorn_diff._run_function
states = []


def capture(*args, **kwargs):
    state = old_run(*args, **kwargs)
    states.append(state)
    return state


unicorn_diff._run_function = capture
with tempfile.TemporaryDirectory() as temporary:
    for name, pairs in (
        ("math64_subtract", [(0x9abcdef012345678, 0xdef09abc56781234),
                             (1, 2), (0x100000000, 0xffff)]),
        ("math64_divide", [(7, 100), (5, 17), (100, 7)]),
    ):
        for a, b in pairs:
            inputs = {
                "a" if name == "math64_subtract" else "numerator":
                    a.to_bytes(8, "little").hex(),
                "b" if name == "math64_subtract" else "denominator":
                    b.to_bytes(8, "little").hex(),
            }
            if name == "math64_subtract":
                inputs["result"] = "00" * 8
            else:
                inputs["quotient"] = "00" * 8
                inputs["remainder"] = "00" * 8
            snapshot = Path(temporary) / f"{name}_{a}_{b}.json"
            snapshot.write_text(json.dumps({
                "description": f"{name} concrete key arithmetic",
                "build_label": "synthetic", "regions": {},
                "arg_overrides": inputs,
            }))
            states.clear()
            sys.argv = ["unicorn_diff.py", name, "--allow-stubs",
                        "--real-callees", "--oracle-native-callees",
                        "--state-snapshot", str(snapshot), "--seeds", "1",
                        "--no-concolic", "--no-leaf-cache", "-q"]
            with contextlib.redirect_stdout(io.StringIO()):
                try:
                    unicorn_diff.main()
                except SystemExit:
                    pass
            if len(states) != 2 or states[0].error or states[1].error:
                raise AssertionError((name, a, b, [state.error for state in states]))
            scratch = states[1].scratch_data
            if name == "math64_subtract":
                actual = int.from_bytes(scratch[0x800:0x808], "little")
                expected = (a - b) & MASK64
                if actual != expected:
                    raise AssertionError((name, a, b, actual, expected))
            else:
                quotient, remainder = divmod(b, a)
                actual = (int.from_bytes(scratch[0x800:0x808], "little"),
                          int.from_bytes(scratch[0xc00:0xc08], "little"))
                if actual != (quotient, remainder):
                    raise AssertionError((name, a, b, actual,
                                          (quotient, remainder)))
            print(name, hex(a), hex(b), "matches")
