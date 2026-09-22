# Lift and Implementation Rules

Load this module for C source, `kb.json`, reverse engineering, lifting, ABI,
types, structs, or Ghidra work.

## Core Implementation Rules

- Lifted code is C89: declarations must precede statements in every block.
- Do not invent behavior or names without binary evidence.
- Preserve original ABI, layout, side effects, control flow, and inline schedule.
  Do not force, duplicate, suppress, or hand-copy inlining to chase bytes.
- Use engine types (`real`, `boolean`, fixed-width types from `src/types.h`),
  cseries flag/math macros, typed tag/object accessors, and named enum constants.
  Put declarations in their genuine owning header or `kb.json`-generated
  `decl.h`, never an unrelated `.c`.
- Never trade an existing exact/ported match for a new one. Report strict,
  meaningful-exact, padded-exact, and fuzzy results honestly. Park coherent but
  unmatched lifts with a reason.
- Do not fake matching with `volatile`, dead/redundant stores, arbitrary
  barriers/pragmas, raw offsets where proven fields exist, representation
  tricks, undefined behavior, or invented source.
- Register every new `.c` file in `src/CMakeLists.txt`.
- Keep logic changes separate from cleanup and formatting.

## ABI and kb.json

- `@<reg>` annotations are immutable. Never remove or change register
  assignments.
- Add register-argument callees to `kb.json` with their proven `@<reg>` and call
  them by name.
- The build system emits thunks from `kb.json`; do not use inline assembly for
  them.
- Audit every raw XCALL cast against the real return and parameter types.
  Float returns use ST(0), integer returns EAX; float parameters use
  FLD/FSTP stack slots, integer parameters use PUSH. After adding or changing an
  XCALL macro, run `rtk python3 tools/audit/check_xcall_types.py`. Float/integer
  mismatch errors block completion.

## Port Toggle

`"ported": false` is a real behavior toggle. `tools/build/patch.py` skips the
original redirect and writes an implementation-entry JMP back to original code,
so original and lifted callers both reach original behavior. Use it to bisect a
regression, then restore `true`.

`pre-commit-ported-deactivations.sh` blocks new deactivations absent from
`tools/audit/deactivation_allowlist.json`; CI repeats this in `DeactivationGate`.
New allowlist entries need a reason and `--update-allowlist`. Emergency bypasses
are `HALO_ALLOW_DEACTIVATIONS=1 git commit` or `--no-verify`.

## Type Recovery Timing

Recover anything capable of changing emitted instructions during `/lift`, not
post-hoc source recovery:

- Settle interface types, return widths, arity, and register arguments before
  writing the body. Use
  `rtk python3 tools/audit/check_param_types.py --callee 0x<addr>` against the
  pristine XBE and `--check` against `param_type_baseline.json`.
- Recover every touched struct field during the lift. `field_<hex>` marks an
  accessed unknown; `pad_<hex>[n]` marks unobserved bytes.
- Leave names, comments, magic constants, and expression-form cleanup until the
  score settles, then use `/recover-source` behind byte-identical gates.

VC71's mnemonic-only LCS can miss severe type bugs, including float/integer
parameter mismatches and fields read through the wrong scalar type. Score alone
cannot validate types.

## Raw Offsets and Producers

A raw offset often indicates an untyped producer rather than a missing field.
For example, a `char *` return declaration on `object_get_and_verify_type`
forces every caller into offsets. Correct a proven producer return type when
codegen-neutral. Generic accessors such as `datum_get` and `tag_get` genuinely
return `void *`; create typed wrappers instead. Census command:

```bash
rtk python3 tools/audit/check_readability.py --untyped-producer
```

## Compiler Runtime Intrinsics

Never transcribe these Ghidra pseudo-calls into C or add them to `kb.json`:

| Address | Intrinsic | Write instead |
|---------|-----------|---------------|
| `0x1d90e0` | `_chkstk` | Declare locals normally; use static storage only when evidence requires it |
| `0x1d9068` | `_ftol2` | `(int)float_expr` |
| `0x1dd5c8` | `__SEH_prolog` | Native `__try/__except` |
| `0x1dd601` | `__SEH_epilog` | Paired native `__try/__except` handling |
| `0x1dd620` | `_allmul` | `(int64_t)a * b` |
| `0x1dd660` | `_aullshr` | `(uint64_t)val >> shift` |
| `0x1dd680` | `_aullrem` | `(uint64_t)a % b` |
| `0x1dd770` | `_aulldiv` | `(uint64_t)a / b` |

See skill `lift-decompiler-traps` section 5 and `docs/seh-handling.md` for SEH.

## Inline Assembly

Clang defines `_MSC_VER` for `-target i386-pc-win32`, but MSVC-style
`__asm {}` does not expose GPR clobbers to clang. Guard it with
`#if defined(_MSC_VER) && !defined(__clang__)`; provide GCC-style
`asm volatile` with proper outputs and clobbers in the alternative branch.
Treat `RDTSC`, `CPUID`, `MUL`, and `DIV` as dangerous because they overwrite
implicit GPRs. FPU-only instructions do not clobber GPRs.

## Decompiler and Stack Hazards

- Verify callee buffer sizes from the callee's `memset` or initializer in
  disassembly; Ghidra can undersize locals.
- An unlifted indirect callee may rely on MSVC local-array overlap. Clang stack
  layout can leave expected adjacent offsets uninitialized. Follow
  `lift-decompiler-traps` section 5.
- Verify every call site against disassembly. The decompiler is a draft:
  register aliases can be wrong; float arguments can appear as PUSH dummies;
  reordered stores can rotate fields; cross-product operands can swap; and
  stack buffer members can appear as unrelated locals.
- Trace PUSH values backward from CALL, derive offsets from EBP-relative MOVs,
  verify cross-product subtraction order, and classify buffer offsets relative
  to the buffer base and proven size.

## Auto-Lift Ownership

`tools/llm_auto_lift.py` owns selection, liftability scoring, and Ghidra context
caching. `/lift` owns code generation. Legacy `review` and `promote` subcommands
exist only for old artifacts.
