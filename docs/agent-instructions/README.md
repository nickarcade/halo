# Agent Instruction Modules

`AGENTS.md` and `CLAUDE.md` contain always-loaded repository essentials. This
directory contains binding detail loaded by task type. Both root files index the
same modules and must remain identical.

## Index

| File | Scope | Load when |
|------|-------|-----------|
| [`research-and-tooling.md`](research-and-tooling.md) | Read discipline, research gate, file caps, RTK, Ghidra access, repository guardrails | Every repository task |
| [`lift-implementation.md`](lift-implementation.md) | C89, ABI, kb.json, types, structs, compiler/runtime traps, call-site validation | Editing C or kb.json; RE or lift work |
| [`build-and-verification.md`](build-and-verification.md) | Build, audits, hazard scans, VC71, equivalence, runtime validation, failure triage | Editing or validating source; build/deploy work |
| [`commits-skills-and-reporting.md`](commits-skills-and-reporting.md) | Lift commit format, automation policy, skill routing, report schema | Committing, delegating, routing, or reporting |

## Maintenance

- Keep durable shared doctrine here, not duplicated in agent runtime trees.
- Keep `AGENTS.md` and `CLAUDE.md` byte-identical and below 500 lines.
- Keep root files limited to mission, always-on invariants, and this task index.
- Add new detailed rules to the narrowest matching module. Update both root
  indexes only when a new module or task category is introduced.
- Shared commands and skills follow [`docs/agent-content.md`](../agent-content.md).
- Command selection belongs in skill `tool-reference`, not this index.
