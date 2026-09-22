# Research and Tooling Rules

These rules apply to every repository task.

## Read Discipline

- Never read the same file twice in one task. Keep a mental ledger.
- A successful edit confirms its result; do not reread to verify it.
- If an edit fails, reread only the failing range, at most 20 lines.
- Files over 300 lines require ranged reads. Limit each read to 100 lines.
- Need an already-read fact? Recall it or use `rtk rg` for the exact string.
- Be brief, natural, and concise. Avoid filler and dramatic phrasing.
- Treat warnings from `tools/audit/token_discipline_hook.py` as authoritative.

## Pre-Edit Research Gate

Target at least four reads or evidence checks per edit. Before editing a
function:

1. Run `rtk rg '<function_name>' src/` for callers and related symbols.
2. Run `rtk read <file> -o <start> -l <limit>` for exact target lines.
3. Run `rtk jq '<filter>' kb.json` for signature and ABI evidence.
4. Run `rtk git diff -- src/ kb.json CMakeLists.txt` for current scoped changes.

Do more research if these do not establish behavior and blast radius.

## File Bans and Caps

| Path | Rule | Reason |
|------|------|--------|
| `kb.json` | `rtk jq` only; never Read or `python -c` | Large structured source of ABI truth |
| `objects.c`, `units.c`, `sound_manager.c` | Ranged reads, at most 100 lines | Large source files |
| `build/`, `build_debug/`, `node_modules/`, `.git/`, `halo-patched/`, `__pycache__/`, `dist/` | Never read or search unless explicitly asked | Generated/noisy content |
| `*.log` | Never read | Rerun producer with narrow output |

## Research Workflow

- Start with exact paths, symbols, and line ranges.
- Before debugging regressions, crashes, hangs, asserts, visual bugs, wrong
  behavior, or build/deploy failures, run
  `rtk python3 tools/memory/prior_fixes.py "<symptom or target>"`. Treat matches
  as hypotheses until binary or runtime evidence confirms them.
- Before source edits, inspect all callers and related symbols.
- Prefer CodeGraph before grep/read when the repository index covers the task.
- Use `rtk fd` for files, `rtk rg` for text, `rtk ast-grep` for structure, and
  `rtk fzf` for selection. If `rtk rg` returns empty or rejects flags, fall back
  immediately to bare `grep -rn`; do not retry flag variants.

## Ghidra Discipline

Before any `ghidra` or `ghidra-live` MCP call, run:

```bash
rtk python3 tools/audit/check_ghidra_mcp.py
```

If it fails, alert the user and stop. Never dump full decompilation or
disassembly speculatively. Query callees first, inspect the smallest useful
target, use `read_memory` on exact ranges, and never redecompile a function that
already has ported C. After Ghidra evidence informs edits, retain first-read line
ranges and use later reads of at most 40 lines; never restart at offset zero.

## RTK and Diffs

- Prefix every shell command with `rtk`, including each command joined by `&&`.
- Common forms: `rtk cmake`, `rtk pytest`, `rtk git status`, `rtk git diff`,
  `rtk read`, `rtk rg`, and `rtk python3`.
- Scope diffs to relevant source: `rtk git diff -- src/ kb.json CMakeLists.txt`.
  Bare diffs can include tracked generated artifacts.
- Use `rtk gain` to inspect token savings.
