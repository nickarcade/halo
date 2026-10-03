# Documentation index

Living documents only. Finished handovers, plans and resolved investigations
are in [history/](history/).

## Guides

- [agent-instructions/](agent-instructions/README.md) - binding rules for coding agents, split by task type
- [agent-content.md](agent-content.md) - how the shared agent commands and skills are laid out
- [lift_pipeline.md](lift_pipeline.md) - input/output contract of the lift pipeline lane
- [lift-policy.md](lift-policy.md) - thresholds, escalation and briefing templates for lift orchestration
- [lift-learnings.md](lift-learnings.md) - bug classes found after activating lifted code, with detectors
- [verification_explained.md](verification_explained.md) - beginner guide to how lift verification works
- [verification_policy.md](verification_policy.md) - when a lift may be accepted with a low match score
- [testing_plan.md](testing_plan.md) - test strategy for the verification stack
- [equivalence-testing.md](equivalence-testing.md) - Unicorn-based differential equivalence testing
- [z3-equivalence.md](z3-equivalence.md) - Z3 + Unicorn extensions to the equivalence harness
- [snapshot-verification.md](snapshot-verification.md) - checking ported functions against captured game state
- [byte-regression-ci.md](byte-regression-ci.md) - raw-XBE byte regression gate in CI
- [vc71-byte-accuracy-playbook.md](vc71-byte-accuracy-playbook.md) - pointer to the VC71 score-recovery reference
- [vc71-low-score-census.md](vc71-low-score-census.md) - census of low-scoring VC71 functions
- [permuter-adapter.md](permuter-adapter.md) - decomp-permuter wiring for VC71
- [structize.md](structize.md) - mechanised struct recovery
- [progress-reporting.md](progress-reporting.md) - progress dashboard design
- [self-hosted-runner-setup.md](self-hosted-runner-setup.md) - self-hosted GitHub Actions runner setup
- [ghidra-live-delinker.md](ghidra-live-delinker.md) - live Ghidra MCP delinker
- [xbdm.md](xbdm.md) - XBDM/RDCP workflow and real-Xbox probing
- [xemu-bridged-deploy.md](xemu-bridged-deploy.md) - deploying to the two bridged xemu guests
- [xbox-pad.md](xbox-pad.md) - virtual controller for xemu
- [input-fixture-capture.md](input-fixture-capture.md) - recording and replaying controller input fixtures
- [ab-trajectory-testing.md](ab-trajectory-testing.md) - patched vs unpatched trajectory diffing
- [boot-init-and-checkpoints.md](boot-init-and-checkpoints.md) - init.txt and saved states
- [assertion_tripwire.md](assertion_tripwire.md) - GDB tripwire on the live assertion funnel
- [halorec-timeseries-leverage.md](halorec-timeseries-leverage.md) - using .halorec recordings as time series
- [rng-trace.md](rng-trace.md) - RNG draw tracing

## Reference

- [compiler-provenance.md](compiler-provenance.md) - compiler and flags identified for build 2276
- [seh-handling.md](seh-handling.md) - compact SEH in lifted code
- [system-link-architecture.md](system-link-architecture.md) - networking layers and session flow
- [debug-commands-keyboard.md](debug-commands-keyboard.md) - debug commands, cheats and keyboard input
- [debug-command-catalog.md](debug-command-catalog.md) - generated debug command catalog
- [halo_ce_original_xbox_bugs_and_glitches.md](halo_ce_original_xbox_bugs_and_glitches.md) - known original-game bugs
- [raw-xbe-oracle-migration.md](raw-xbe-oracle-migration.md) and [raw-xbe-oracle-worklog.md](raw-xbe-oracle-worklog.md) - raw-XBE oracle migration status and log
- [references/](references/) - agent reference policies (ABI, kb updates, prototypes, output schema)

## Open bugs and investigations

- [bugs/mp-endgame-freeze.md](bugs/mp-endgame-freeze.md) - soft-deadlock when a multiplayer match ends
- [bugs/mp-dead-biped-limp.md](bugs/mp-dead-biped-limp.md) - dead bipeds linger; limp flag never sets
- [bugs/texture-cache-corruption.md](bugs/texture-cache-corruption.md) - texture banding and corruption
- [plasma-orb-bug.md](plasma-orb-bug.md) - invisible charged plasma-pistol orb
- [cryo-tech-no-move-handoff.md](cryo-tech-no-move-handoff.md) - cryo tech NPC does not run to the door
- [system-link-rng-desync.md](system-link-rng-desync.md) and [system-link-desync-handoff.md](system-link-desync-handoff.md) - system-link desync

## History

[history/](history/) holds finished handovers, plans, resolved bug write-ups and
old investigation logs. They record what was known at the time and may be stale.
