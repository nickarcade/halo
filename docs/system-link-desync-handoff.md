# System-link desync handoff — 2026-09-22

## Objective

Identify the first causal simulation difference between a patched client and
an original-gameplay debug-2276 host. Keep investigation cost low: one bounded
paired capture, offline analysis, then one evidence-directed test.

## Current state and constraints

- **OPEN: no proven desync root cause or verified fix.** This session performed
  read-only log/source/binary checks, then documentation edits only.
- User reports client `DECOMP BUILD c2afdbd63 (2026-09-22T14:44:05.045027)`
  against a `cachebeta` host. The client log contains that label. Neither
  running image was independently verified. A build label is not a file hash
  or proof of the exact source/configuration used to compile it.
- Grenades accelerate reproduction but are neither necessary nor guaranteed
  to trigger the first failure. Failures also occur during ordinary play.
- User explicitly wants low token expenditure and notification **before any
  implementation changes**. Explain the exact diagnostic changes first.
  This handoff does not authorize an unattended deployment/capture campaign.
- No guest connections, builds, deployments, cache clearing, source edits or
  `kb.json` edits occurred. Before documentation edits, only `README.md` was
  dirty; that unrelated edit must be preserved. Recheck status on resume.
- Read [the historical evidence](system-link-rng-desync.md) selectively.
  [Architecture](system-link-architecture.md) maps the current network layers.
  `networking_system_link_bug.md` predates the transport lift; its old revert
  list and pregame theory are not current root-cause findings.

## Confirmed observations

Logs supplied by the user (mutable files; preserve a copy before a new run):

- Client: `G:/dev/halo/xbdm/debug_client.txt`
  (`/mnt/g/dev/halo/xbdm/debug_client.txt` in WSL).
- Matching host log found at `G:/dev/halo/xbdm/debug_host.txt`.
  The supplied `/mnt/g/dev/xbdm/debug_host.txt` path was absent.
  The discovered file contains older sessions too; select September 22.

| Client log time | First reported mismatch in that match |
| --- | --- |
| 14:15:48 | tick 14467, seeds `65836381/312dd6fd` |
| 14:38:09 | tick 208, seeds `cbd3c722/8f1ed6a4` |
| 14:49:02 | tick 435, seeds `671d7521/38d5af58` |

The host reports the last client's failure at tick 437 (14:49:03). These are
detection points, not proof of the initiating divergence tick.

`network_game_client_handle_game_update` (0x125380, source around line 887)
compares seeds when packet game time equals local game time and the expected
update sequence matches. It calls the out-of-sync handler on mismatch. The
nearby periodic "lagging behind" message merely logs the update/time difference.
The user also sees 1-2 tick lag with original/original; that alone is not failure.

## Suspicious functions: evidence and limits

Port flags below were checked in current `kb.json`; they do not verify a live XBE.
No new instruction-level defect causing gameplay desync was established here.

| Priority / target | Why inspect it | What is not established |
| --- | --- | --- |
| First causal boundary: `object_try_place` (0x1443f0), reached from `unit_throw_grenade_release` (0x1ab110); both active | Historical ds101 retained equal sampled player translations and grenade-placement rays, but client hit biped `e2770008`, surface 21, while host had no hit | Equal rays do not prove equal transforms, geometry, flags or other collision state. Neither function is proven faulty. Grenades may amplify an earlier difference. |
| Upstream producer: `FUN_001a2f40` (0x1a2f40), **inactive** | Ds93 localized early player-position drift to its output; restoring its original body made sampled early translations exact in ds94 | Its fallback was not a complete fix; later actual failures remained. Do not re-enable it or call it the sole cause. |
| Active helpers: `FUN_00012170` (0x12170), `distance_squared3d` (0x121a0), `magnitude3d` (0x12f10), `vector3d_scale_add` (0x12f80), `FUN_00012fe0` (0x12fe0), `normalize3d` (0x13010) | Historical call graph connects these to original `FUN_001a2f40`; they also survived ds107's partial bisection | Static reachability is not execution evidence. x87 precision/order is a hypothesis; no defect in these six was demonstrated. Inspect only after a capture implicates a call. |
| Collision callees: `FUN_0014dce0` (0x14dce0), `FUN_0014cb00` (0x14cb00), `collision_bsp_test_vector` (0x149480); active | Relevant collision chain | Earlier individual restorations did not remove disagreement. This lowers priority for repeating those exact tests, but does not clear their inputs/callees. |
| `matrix_transform_point` (0x109590), active; node-matrix producers | A previous expression-order correction was binary-backed; matrices differed in some captures | That correction and tested node-chain restorations did not resolve the issue. Matching fingerprints or synthetic tests did not establish all live inputs/alias cases. |

Treat the RNG generator and the mismatch reporter as observation points first.
Historical captures show collision/damage differences preceding different draw
sequences. A transport/input fault remains possible if applied actions differ.

## Separate join failure: empty map name

At 14:47:45 the client logs `couldn't find map '' on the DVD` before any logged
game-settings update in that join, followed by `XLaunchNewImage`. The host had
Carousel selected at 14:47:41 and accepted the client at 14:47:45. A subsequent
join at 14:48:36 successfully precached Carousel, then desynced during gameplay.

- `network_game_client_idle_pregame` (0x126ce0) calls
  `network_game_client_update_precache_status` (0x126000) before servicing
  incoming messages. The latter periodically passes
  `main_get_multiplayer_map_name()` to `cache_files_give_time_to_precache`
  (0x1b9de0). Game-settings handling sets the map name.
- **Binary check:** original 2276 also uses that precache-before-receive order;
  the sampled timer/cache-service branches agree with current source. Do not
  blindly reorder calls or add an empty-name guard and call it a faithful fix.
- **Hypothesis:** precaching runs before initial settings supply a map name.
  `FUN_001bd1b0` (0x1bd1b0) compares against six cached names without rejecting
  an empty request. An empty slot can accidentally satisfy that request;
  having six nonempty names could expose the failure more often. Cache contents
  and the actual failing call stack have not been captured.
- The raw zero-argument call to register-argument `FUN_001bd1b0` in the C body
  of `cache_files_precache_map_begin` is suspicious, but **that body is inactive**
  (`ported:false`, 0x1bd910). It is not evidence of an executing fault.
- Bounded follow-up: at an empty precache request capture the caller, client
  state, last settings/timer state, map buffer and six cached names. Preserve
  cache contents. No connection to gameplay RNG desync has been demonstrated.

## Existing capture tools and gaps

- `src/halo/math/random_math.c:30`: `rng_trace_note`; default filter at line 40
  **drops RNG draws/reseeds**, retaining only kinds 83-97 unless
  `HALO_RNG_TRACE_DEEP` is defined. The old `--rng-trace` recipe is insufficient.
- `src/halo/math/rng_trace.h`: 65536 records, 16 bytes each, approximately 1 MiB.
  Existing header has magic/version/capacity/write index, **no freeze state**.
  A long/busy match can overwrite the causal interval. Starting at match start
  does not mean that the whole match remains in the ring.
- `artifacts/rng_trace/build_original_probe_focused.py` currently restores
  v19 detours except biped/translation sites and writes
  `host_rng_probe_biped_focused.xbe`. Do not assume it rebuilds the older
  `host_rng_probe_focused.xbe` or supplies matched RNG coverage.
- `tools/xbox/rng_trace_dump.py` supports `--host`, `--hmp-port`, `--pe`,
  `--original-xbe`, `--runtime-base`, `--out`. Its defaults refer to mutable build
  outputs. Pass frozen matching artifacts. Host ring addresses/layout must come
  from the matching host probe manifest; do not use client PE addresses on host.
- `tools/xbox/rng_first_divergence.py` compares the union of ticks. Missing
  capture coverage can appear to be divergence. Default latest-segment selection
  is not proof that the same map epoch was selected.
- `artifacts/rng_trace/compare_pair.py` checks RNG continuity and aligns by a
  common reseed, falling back to a unique shared seed. Its shared-prefix result
  is not a completed-match pass; seed equality alone is not epoch identity.
- The old float/translation pause watcher is **not** the desync oracle. Do not
  resume it or old function-count bisection as the default next step.

## Next steps: bounded paired capture (proposed, not implemented)

1. **Freeze provenance and announce scope.** Save baseline XBE, matching PE,
   source revision/dirty diff and build configuration; hash exact files. Confirm
   current guest roles/endpoints without restarting play. Historical addresses
   were client `10.0.0.21`, host `10.0.0.25`, HMP ports 4444/4446; recheck them.
   Explain diagnostic edits before making them. Preserve unrelated work.
2. **Prepare matched low-volume tracing.** Retain global RNG draws/reseeds and
   a bounded, explicitly listed set of existing state probes on both peers.
   Preserve original gameplay bodies on the diagnostic host; it is instrumented,
   not byte-for-byte stock. Reuse audited probes where possible. Avoid enabling
   every deep probe merely to bypass the filter. Verify original bytes, hook
   boundaries, register/flags/x87 preservation and caller symbolization.
3. **Implement trace freezing, not game pausing.** Freeze the client ring on
   the first actual seed-mismatch branch before out-of-sync handling. Freeze the
   host ring when its original client-update handling recognizes that client's
   out-of-sync report. Verify that host hook against binary first. Record reason,
   match identity, detection tick, compared seeds, final committed index and wrap
   information. Update schema/dump tooling consistently. Stop recording only;
   retain the game's original error handling and RNG behavior. Provide an
   explicit end-of-test freeze for controls. If either capture is incomplete or
   the host never freezes, report that rather than silently trusting its dump.
4. **Validate instrumentation before deployment.** Check that the selected
   event kinds are actually recorded, freeze stops writes without altering
   gameplay state, and offline comparison handles truncated/partial ticks,
   different epochs, unequal ring coverage and real extra/missing draws. Store
   a manifest per peer: image/symbol hashes, ring VA/schema/filter, hooks and
   expected live bytes. Complete upload, launch acknowledgement and live byte
   verification before asking the user to play. Do not edit HDD `init.txt`.
5. **One brief A/A control.** Both peers run the same original-gameplay diagnostic
   image. Exercise movement, shooting and grenades; explicitly end the trial.
   Compare complete overlapping ticks and continuity. This is an instrumentation
   sanity check, not proof against every slower failure. Before later claiming
   causal isolation, obtain a control at least as long/exercised as the failure.
6. **One patched-client/original-gameplay-host match.** Fresh Carousel match;
   record player/controller count, variant and grenade types. Play normally,
   throwing grenades periodically. Set an explicit time limit with the user;
   completion without failure is inconclusive. During play: no builds, uploads,
   debugger polling, or early stops for small float differences. After actual
   desync/freeze, retrieve each ring once and save both logs and manifests.
7. **Analyze offline and stop to report.** Validate runtime identity, RNG
   continuity, wrap/coverage and matching map epoch. Compare simulation ticks,
   event kinds, normalized callers and per-call occurrences; account for partial
   boundary ticks. Output only last agreement, first RNG sequence difference,
   preceding retained state differences, and one proposed next experiment.
   If the needed prehistory is absent, call the capture inconclusive and adjust
   retention/probe scope before another run. Do not label a later mismatch first.
8. **Follow the causal boundary.** Different applied actions -> update/input
   path. Same actions but different collision inputs -> their state producer.
   Equal complete relevant inputs but different collision output -> compare the
   original and candidate on cloned live state, restoring all affected state
   between calls. A ray alone is not complete input. Add only missing probes
   around that boundary. A higher VC71 score or a slower failure is not a fix.

## Validation and implementation status

- `rtk python3 -X utf8 tools/audit/check_ghidra_mcp.py` passed. Ghidra identified
  `cachebeta.xbe`. Scoped original-byte reads: `0x126000` (96 bytes), `0x126d20`
  (96), `0x1b9de0` (135), `0x1bd1b0` (86); callees queried first. No Ghidra edits.
- Prior-fix searches and scoped source/`rtk jq` checks performed. The broad
  `check_callee_reg_args.py` run was interrupted without results; it did not pass.
- No runtime capture, live-image check, build, VC71 or equivalence test was run.
  Automatic freeze and capture/comparison fixes above remain **proposed**.
- These docs do not justify a gameplay patch or `kb.json` change. Apply repo
  debug/ABI/verification skills to whichever bounded function evidence selects.

## Resume prompt

> Read `docs/system-link-desync-handoff.md` first. Investigate patched-client /
> cachebeta-host desync with low token use. Grenades accelerate but do not define
> the failure. Preserve historical findings without treating them as proven
> causes. Before changing anything, summarize the smallest diagnostic-only patch
> for matched RNG/state capture and actual-desync trace freezing. The current
> default tracer suppresses RNG events. Prepare a reviewable capture setup, then
> coordinate one brief A/A control and one failing match with verified image
> identities; no unattended bisection or deployments during play. Analyze offline
> and report one causal boundary/next test. Keep the empty-map join issue separate.
