# Agent Instruction Modules

`AGENTS.md` contains always-loaded repository essentials. This directory
contains binding detail loaded by task type. `CLAUDE.md` is not tracked; create
it locally as a symlink to `AGENTS.md`.

## Index

| File | Scope | Load when |
|------|-------|-----------|
| [`research-and-tooling.md`](research-and-tooling.md) | Read discipline, research gate, file caps, RTK, Ghidra access, repository guardrails | Every repository task |
| [`lift-implementation.md`](lift-implementation.md) | C89, ABI, kb.json, types, structs, compiler/runtime traps, call-site validation | Editing C or kb.json; RE or lift work |
| [`build-and-verification.md`](build-and-verification.md) | Build, audits, hazard scans, VC71, equivalence, runtime validation, failure triage | Editing or validating source; build/deploy work |
| [`commits-skills-and-reporting.md`](commits-skills-and-reporting.md) | Lift commit format, automation policy, skill routing, report schema | Committing, delegating, routing, or reporting |

## Maintenance

- Keep durable shared doctrine here, not duplicated in agent runtime trees.
- Keep `AGENTS.md` below 300 lines (enforced by `tools/audit/check_agent_docs.py`).
- Keep root files limited to mission, always-on invariants, and this task index.
- Add new detailed rules to the narrowest matching module. Update the root
  index only when a new module or task category is introduced.
- Shared commands and skills follow [`docs/agent-content.md`](../agent-content.md).
- Command selection belongs in skill `tool-reference`, not this index.
