# System Link Architecture

Sources: current `kb.json` function inventory, the networking source under
`src/halo/networking/` and `src/halo/bungie_net/`,
`artifacts/ntsc_callgraph/callgraph.json`, and
`docs/system-link-rng-desync.md`. The port-status snapshot below was checked
on 2026-09-24. Historical investigation notes are useful evidence, but do not
override current source or `kb.json`.

# 1. Current port coverage

`ported:true` means the patched image redirects to the C implementation.
`ported:false` means the C body is retained but the patcher routes both entry
paths to the original body. An omitted `ported` field likewise leaves the
original body active.

| Object                               | Active redirects | Original body active | Notes                                                                                                                                                                |
|--------------------------------------|-----------------:|---------------------:|----------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `transport_endpoint_set_winsock.obj` |            53/53 |                    0 | Transport and endpoint-set lift is fully active.                                                                                                                     |
| `message_header.obj`                 |            23/23 |                    0 | Four nested object entries omit `ported`, but their top-level KB entries set `ported:true`. A rebuilt XBE confirms active redirects at 0x803d0, 0x80940, 0x80a40, and 0x80d50. |
| `network_server_manager.obj`         |            84/84 |                    0 | All server-state routines have active redirects.                                                                                                                     |

## 1a. Activation evidence and limits

All functions in the 15-object system-link inventory now have C bodies and
active redirects in a freshly built patched XBE. This records repository and
local build state; it does not identify an already deployed XBE. Other objects
outside this scoped inventory can still contain deactivated functions. VC71
scores below are mnemonic and operand comparisons against XBE-synthesized
references, not runtime proof.

`FUN_00083930` (socket creation), `FUN_00083e20` (bind),
`FUN_000841b0` (asynchronous connect), `FUN_000843a0` (listen), and
`FUN_00084740` (datagram send) are now active transport redirects.

`network_game_server_setup_game_from_playlist` is active. The recovered `network_game_blob_t` identifies the 16-wide-character game name at `+0`, map version at `+0x20`, and maximum teams at `+0x10f`; PAL and 2276 agree. Typed accesses preserve 84.7% mnemonic and 72.7% operand match. The default synthetic differential is vacuous at 44.7% coverage, so the lift pipeline's PASS is not an equivalence verdict. The tracked [`check_system_link_setup_playlist.py`](../tools/equivalence/check_system_link_setup_playlist.py) checks playlist failure and team/non-team success with nonzero input fields. Success reaches 74.5% of the original body. A code hook confirms the same generated wide name reaches `ustrncpy`, and the full 0x4c0-byte final server state matches the original, including game name, map version, player limits, team limit, and open-game flag. The oracle's open-game helper and both sides' `ustrncpy` are intercepted in the fixture; their Ghidra-confirmed writes are modeled explicitly. The candidate's local default-name initialization uses `csmemcpy`/`csmemset` where the oracle initializes inline. Live playlist behavior remains unverified.

The broadcast-search handler has a typed 0x114-byte game-advertisement payload. PAL's field order matches the 2276 writes at `+0x34` (port), `+0x3a` (game name), `+0x74` (map), `+0xf8` (engine type), `+0x102` (flags), and `+0x104` (join token); compile-time size and offset checks pin that layout. Its writes use the named fields, while key and XNADDR copies keep their original four-byte operations. VC71 remains 89.7% mnemonic and 80.2% operand match, and the whole server-source regression gate passes. The lift pipeline passes 100/100 default synthetic seeds but covers only 13.2% of the handler. A tracked valid-search snapshot reaches 87.0% of the 2276 handler; [`check_system_link_advertisement.py`](../tools/equivalence/check_system_link_advertisement.py) confirms all 0x114 payload bytes plus the connection handle, effective send size, broadcast destination, address length, and port agree at the send call. Ghidra at `0x12d250` confirms the original join-token helper copies the first 16 bytes of `message in a bottle`; the snapshot supplies that effect to the oracle while the candidate inlines it. Separate creation-failure, write-failure, and closed-game snapshots pass at 83.0%, 89.2%, and 86.3% coverage, respectively. After those checks, `0x12f690` was enabled (`ported:true`). Message creation, network delivery, and live system-link behavior remain unverified.

The ping reply C body previously wrote IPv4 address length at reply-address `+2` and port at `+4`, overwriting source address bytes. The 2276 instructions write length at `+0x10` and port at `+0x12`, matching PAL and the verified `transport_address` layout. Separating PAL's four-byte pong payload from the typed reply address raises VC71 mnemonic match from 80.0% to 94.0% and operand match from 66.2% to 80.8%, without a whole-server-source regression. The relocation-aware structural comparison improves from 96/208 to 115/180 aligned bytes; literal linked-byte identity remains unverified. The tracked [`check_system_link_ping.py`](../tools/equivalence/check_system_link_ping.py) probe captures both sides at message creation and network write: timestamp, message type, connection handle, effective 16-bit send size, IPv4 destination, address length, and port all agree. Its successful-send case was rerun after the source change and reaches 64.6% of the oracle body; prior creation and write failure snapshots agreed at 40.7% and 71.4% coverage but were not rerun here. The network helper itself remains stubbed. `0x12f8d0` is enabled (`ported:true`), and the freshly built XBE has a redirect at that address. Live system-link behavior is still unverified.

PAL's `message_client_ping` is an eight-byte payload with timestamp at `+0`, port at `+4`, and two padding bytes; the 2276 handler uses those widths and offsets. A local checked type now replaces the raw decoded-buffer accesses without changing the ping handler's VC71 scores.

On the client pong consumer, the default Unicorn pointer slot is only `0x400` bytes, while the ping target, sample count, average, and listening flag are at client offsets `+0x808` through `+0x82a`. A one-off run with `abi.POINTER_SLOT=0x1000` and a synthetic client pointer payload reached the average-update path; oracle and candidate agreed with 60.7% function coverage. A future-timestamp path also agreed. The ordinary 100-seed run reached only 27.7%, so it does not establish these stateful paths by itself.

`network_game_server_idle` is active. PAL and 2276 agree on transport availability, game validity, connection idle, public endpoint, client-machine handling, and pregame/ingame/postgame dispatch. PAL assigns the return value from `network_server_close_client_connection` after a rejected new client; Ghidra confirms 2276 ignores that return, as the C implementation does. VC71 scores are 98.6% mnemonic and operand match without a whole-server-source regression. The ordinary 100-seed differential passes but covers only 15.4% of the function. The tracked [`check_system_link_server_idle.py`](../tools/equivalence/check_system_link_server_idle.py) runner compares 11 focused states with a full server pointer slot. It checks helper order and arguments, address formatting, error and event text, connection closure, state dispatch, return byte, and the final 0x4c0-byte server state. The probe normalizes `@<eax>`, `@<esi>`, and `@<ebx>` callees; the candidate inlines the postgame helper, so its clock call is matched to the oracle's helper call in the no-heartbeat case. Forcing the client-machine handler to return false diverges, but its 2276 and C bodies return true on all normal paths. Network effects and live system-link behavior remain unverified.

`network_game_server_handle_public_endpoint` now follows PAL's read-loop shape while retaining the 2276 handler result. VC71 mnemonic match rose from 91.7% to 96.2%; the provisional relocation-aware aligned-byte count moved from 110/146 to 111/138. A saved one-datagram snapshot agrees on the handler-failure return, and the 100-seed synthetic run had 99 passes and one oracle memory error. These checks do not establish literal linked-byte identity or live datagram behavior.

`network_game_server_reset_to_pregame` is active. PAL and the 2276 decompile agree on countdown and machine-state clearing, round-counter advance, team rotation, switch-message broadcast, and next-game or playlist-end handling. VC71 scores are 93.4% mnemonic and 82.2% operand match. The default 100-seed differential passes but reaches only 26.4% of the function. The tracked [`check_system_link_reset_pregame.py`](../tools/equivalence/check_system_link_reset_pregame.py) runner uses a full server pointer slot with nonzero counters, flags, and game data. Ten focused branches pass: initial setup, first and second message creation/send failures, next-game success, valid and invalid team rotation, playlist exhaustion, and client-machine failure. The team path reaches 58.7% of the 2276 function. A code hook confirms identical message types and complete 4-byte or 0x434-byte payloads at creation and identical final 0x4c0-byte server state; it also verifies the cleared fields, advanced counter, machine flags, and team assignments against expected values. Setup-from-playlist and client-machine handling are now active. Whole-system live behavior is still unverified.

`network_game_server_handle_client_machines` is active. PAL and the 2276 decompile agree on four server machine slots at `+0x43c` with a `0x10` stride, game-machine records at `+0x11c` with a `0x44` stride, connection state checks, read loop, message dispatch, and removal paths. Restoring PAL's typed slot loop and nested message/removal branches raises VC71 from 77.8% to 96.7% mnemonic and from 70.1% to 74.7% operand match, without a whole-server-source regression. The provisional relocation-aware aligned-byte range rises from 236/476 (49.6-54.6%) to 292/396 (73.7-81.8%); literal linked-byte identity remains unverified. The tracked [`check_system_link_client_machines.py`](../tools/equivalence/check_system_link_client_machines.py) runner checks ten focused cases against the original. Its candidate helper names now come from the live stub map, and both sides use the snapshot's stubbed helper returns; hardcoded candidate sentinel addresses previously misclassified calls after source reshaping. A code hook compares helper call order and arguments, 0x800-byte read capacity on each call, the exact 0x24-byte packet passed to the handler, removal record selection, event text, and final 0x500-byte server state. The two-slot case checks advancement from slot zero to slot one and machine index two. The handler returns true on every path, so the ordinary return-value differential gives little evidence; the focused checks supply the side-effect comparison. Connection, message, and removal helpers are intercepted, and live network behavior remains unverified.

`network_game_server_add_new_client` is active. PAL and the 2276 decompile agree on the open-game check, first free slot selection, 0x18-byte address initialization and validation, remote-address policy, machine invalidation, connection acceptance, and event paths. PAL's four-slot loop and enclosing game-open branch, with an ordinary result local, raise VC71 scores from 82.1% to 84.1% mnemonic and from 63.5% to 67.0% operand match without a whole-server-source regression. Relocation-aware aligned bytes improve from 183/365 to 196/338; literal linked-byte identity remains unverified. The tracked [`check_system_link_add_new_client.py`](../tools/equivalence/check_system_link_add_new_client.py) runner compares eight focused cases, including a full server, invalid and rejected addresses, loopback acceptance, acceptance failure, and slot two. It now resolves candidate stub names from symbols rather than fixed addresses, so the probe remains valid when C code layout changes; all eight cases passed after this edit. It checks normalized helper call order and arguments, the address passed for formatting, full 0x500-byte server state, and the expected connection, index, and flags writes. The ordinary 100-seed differential has 98 passes and two unmapped candidate reads, with only 25.7% default coverage; the focused probe supplies the relevant stateful paths. Address retrieval, connection acceptance, and event delivery remain intercepted, and live join behavior is unverified.

`network_game_server_idle_pregame_tasks` is active. PAL and the 2276 decompile agree on dead-client removal, five-second keepalives, readiness gating, countdown packets, game start, and the 15-second client-loading timeout. Reshaping the countdown and keepalive branches to follow PAL raises VC71 mnemonic match from 79.4% to 94.4% and operand match from 77.2% to 93.5%, with no whole-server-source regression. The relocation-aware structural comparison improves from 399/722 to 478/616 aligned bytes; this is provisional alignment evidence, not literal byte identity. The tracked [`check_system_link_pregame_tasks.py`](../tools/equivalence/check_system_link_pregame_tasks.py) runner compares 14 reachable paths, including message creation and send failures, with helper call order and arguments, two-byte payloads, event text, return byte, and full 0x500-byte server state. It also checks keepalive and countdown timestamps and the cleared invalid-countdown flag. Several one-seed snapshots have low path diversity, so the result supports those specific paths only. The probe normalizes the `@<eax>` callee argument, which the runtime patcher rewrites but the isolated candidate-object emulator passes on the stack. Forcing `network_game_server_start_network_game` to return false produces a candidate-object mismatch because its C body always returns true and the compiler folds the caller's failure check; Ghidra confirms the 2276 callee also returns true on every normal path. Network helpers and live pregame behavior remain unverified.

## 1b. Full pipeline inventory (2276)

The 15 scoped `kb.json` objects below contain 391 functions. All 391 have active redirects. The four message-header entries with no `ported` field in the nested object list have top-level `ported:true` records. A fresh patched XBE byte audit found a `push target; ret` redirect at every one of the 391 entries and no entry with the original six bytes. The earlier 13-object count missed `thread_win32.obj` (thread/mutex and key-agreement helpers) and `64bit_math.obj` (key-agreement arithmetic); both are fully active. The PAL 2342 source provides counterparts for the client/server state machines; its ABI, source shape, and behavior must be checked against the 2276 binary before reuse. Its message-header file lacks the 2276 encryption implementations.

Recheck the built redirects with `rtk python3 tools/audit/check_system_link_redirects.py` after building the `patched_xbe` target. The audit checks all 391 KB activation flags and verifies each 2276 entry contains a `push target; ret` whose target lies in patched executable code. It uses the pristine 2276 XBE only as the original-byte reference. This is build coverage evidence, not live network behavior evidence.

| Pipeline stage | Objects | Active / total |
|---|---|---:|
| Transport | `transport_endpoint_set_winsock.obj`, `transport_address.obj` | 56 / 56 |
| Message framing and crypto | `message_header.obj` | 23 / 23 |
| Key-agreement and platform support | `thread_win32.obj`, `64bit_math.obj` | 20 / 20 |
| Connection | `network_connection.obj` | 18 / 18 |
| Serialization | `data_packet_groups.obj`, `network_messages.obj` | 42 / 42 |
| Client | `network_game_globals.obj`, `network_client_manager.obj`, `network_client_message_handler.obj` | 100 / 100 |
| Server | `network_server_manager.obj`, `network_server_message_handler.obj` | 92 / 92 |
| Game state and lockstep | `network_game_manager.obj`, `game_time.obj` | 40 / 40 |

A static direct-call boundary check against `artifacts/ntsc_callgraph/callgraph.json`
found three original-body game helpers called by the 15 objects. The
good-color picker at `0x1c19a0` is now ported: PAL identifies it as
`player_profile_get_random_good_color`, and 2276 calls
`seed_random_range(local_seed, 0, 3)`. Its C lift has 100% mnemonic and
operand match, passes the object-wide VC71 gate, and has a redirect in the
fresh patched XBE. The random-player-name lookup `FUN_0019d420` is also now
active. PAL and 2276 agree on its UTF-16 tag entry, signed index check, and
in-place terminator; its lift passes 100/100 synthetic seeds at 62.1%
coverage and scores 88.2% mnemonic / 85.3% operand match without an
object-wide regression. The model marker-group lookup `FUN_00123d80` is now
ported too. PAL calls it `model_find_marker`; Ghidra confirms the signed
16-bit binary search over 0x40-byte entries, case-insensitive compare, and
`-1` miss result. Its lift pipeline passes 120/120 synthetic cases at 47.4%
coverage and scores 85.5% mnemonic / 71.8% operand match. The strict
model-source VC71 gate reports two older functions below their recorded
baseline; removing this new body leaves both drops unchanged, so this port
does not cause them. The same boundary audit exposed an original UI helper,
`network_game_reset_to_pregame_ui` at `0xe8830`, called by the client pregame
transition. It is now active. PAL and 2276 agree on the four screen choices
and the connected host's countdown pause. Its lift scores 96.5% mnemonic /
95.1% operand match; a tracked
[`check_system_link_ui_pregame.py`](../tools/equivalence/check_system_link_ui_pregame.py)
runner checks eight success/failure states against the original, including
the widget path, host pause arguments, and error text. The generic 100-seed
differential was vacuous at 31.3% coverage because it stayed on one path.
The strict UI-source VC71 gate reports no regressions but cannot score an
already absent baselined `FUN_000e5180`; it lists 19 unrelated improvements.
All direct game and UI helper callees found by this static boundary check now
have active C bodies.
SDK, XNet, and C runtime calls at this boundary are platform dependencies.
This static index records direct calls only; indirect calls and live behavior
still need separate checks.

PAL review found three distinct cases. The 2276 transport functions `transport_dispose` and `transport_network_available` have low mnemonic percentages because their reference bodies are very short; Ghidra confirms their current behavior, including a `void` return for `transport_dispose` where PAL returns an error code. The client search and server machine handlers have longer low-match bodies: PAL's explicit result handling improves the 2276 client search score, while binary-backed branch and layout recovery improves the server handler's operand match. The two player-request handlers score 100% in both mnemonic and operand comparison and have now passed focused packet and helper checks.

`network_game_client_idle_searching` now follows PAL's single-success-return control flow and zero-initializes the local game record as PAL does, while retaining the 2276 buffer size and call arguments. Its split-screen result check also tests AL, as the 2276 instruction does. Whole-source VC71 matching rose from 82.2% to 94.7% mnemonic and from 77.8% to 88.9% operand, with no other score or warning regression. These are structural scores: the strict raw-byte audit reports `not comparable` because the candidate COFF function has a relocation at `+0xd`. The separate relocation-aware raw-XBE comparison improved from 432/645 (67.0-72.6%) to 447/645 (69.3-74.9%) aligned bytes; both ranges are provisional because nine relocation targets remain unresolved. The pristine caller passes the client pointer in EAX while the current candidate reads a stack argument, so the remaining ABI difference needs separate review. The tracked [`check_system_link_client_search.py`](../tools/equivalence/check_system_link_client_search.py) compares twelve original-versus-candidate states, covering network and idle failure, broadcast and ping success, creation failure, write failure, local-host join success and failure, and a synthetic high-byte split-screen return. It checks packet payloads, send arguments, destinations, returns, client timestamps, and the complete zeroed local game record. Packet creation, connection write, nonce generation, the clock, and the downstream join helper are modeled; live traffic remains unverified.

The 2276 `network_game_client_idle_joining`, `_pregame`, `_ingame`, and `_postgame` functions also test AL after `network_game_is_splitscreen_local`; their former C candidates tested EAX. Byte-width casts restore the four AL tests and add one matching aligned byte in each function (245/334, 99/174, 197/360, and 75/121 respectively). These are provisional relocation-aware comparisons, and the whole client-manager VC71 score gate shows no function-score or warning regression. Each function passes the existing 100-seed differential, but coverage is 18.7%, 41.5%, 16.1%, and 57.3% respectively, with only one return value observed in each run.

The joining path now follows PAL's `success == TRUE` gate, nested join-message success and failure branches, and single result return after incoming-message processing. The 2276 string at `0x292d48` confirms `network_connection_idle() failed in network_game_client_idle_joining()`; the former C named the server-reliable-endpoint helper. VC71 mnemonic match rises from 92.1% to 96.1% and operand match from 90.5% to 94.5%, with no other client-manager score or warning regression. Provisional relocation-aware aligned bytes rise from 245/334 (73.4-79.3%) to 260/331 (78.5-85.8%). The lift pipeline passes ABI, build, hazard, buffer, and VC71 stages. The ordinary 100-seed differential passes but covers only the 18.7% early-exit path. The tracked [`check_system_link_client_joining.py`](../tools/equivalence/check_system_link_client_joining.py) comparison adds three controlled paths: already-sent join request, message creation failure, and 120-second connect timeout. Each passes against 2276, reaches 31.3%, 50.5%, and 32.6% of the original respectively, and the timeout check confirms that both sides clear the connect handle. These one-case synthetic checks do not establish full join behavior or live traffic.

`network_game_client_request_remove_player` now follows PAL's single-result switch, with separate pregame and ingame send branches. Its provisional relocation-aware aligned bytes improve from 251/462 (54.3-63.0%) to 316/455 (69.5-80.0%); literal byte identity is still unverified. VC71 mnemonic match rises from 86.1% to 94.2% and operand match from 70.5% to 78.4%. The strict whole-client-manager gate found no regression among its 60 measured functions, and the lift pipeline passed build, ABI, hazard, and buffer checks. The ordinary 100-seed differential passes but covers only 13.5%. The tracked [`check_system_link_remove_player.py`](../tools/equivalence/check_system_link_remove_player.py) runner compares five controlled states with 2276: pregame and ingame send success, postgame write failure, pregame creation failure, and searching-state rejection. All five pass, reaching 25.0-39.1% of the original separately. Connection write and message creation are stubbed, so payload delivery and live player removal remain unverified.

PAL's result flow also improves `network_game_client_idle_pregame` without changing its 2276 calls or field offsets. The 2276 event string on connection-idle failure is `network_connection_idle() failed in network_game_client_idle_pregame()`, matching PAL; the prior C named a different helper. VC71 mnemonic match rises from 80.0% to 95.8% and operand match from 68.1% to 94.4%, with no other client-manager score or warning regression. The provisional relocation-aware comparison rises from 99/174 (56.9-61.5%) to 125/167 (74.9-82.0%) aligned bytes. The tracked [`check_system_link_client_pregame.py`](../tools/equivalence/check_system_link_client_pregame.py) passes seven original-versus-candidate states, checks event text, helper arguments, returns, and client state, and reaches all 72 reference instruction addresses across the cases. Network delivery and downstream helpers are modeled; the candidate inlines the message-processing loop, whose binary-proven inner failure event the probe accounts for explicitly.

PAL's single `success` result and branch structure also improve `network_game_client_idle_ingame`. The 2276 binary contains PAL's `network_connection_idle() failed in network_game_client_idle_ingame()` event text; the former C named the server-reliable-endpoint helper instead. VC71 mnemonic match rises from 86.3% to 99.2% and operand match from 69.8% to 92.4%, without another client-manager score or warning regression. The current relocation-aware audit aligns 246/318 bytes (provisional 77.4-84.9%); no pre-edit aligned-byte baseline was obtained because that separate VC71 invocation failed with a WSL socket error. The lift pipeline passes ABI, build, hazard, buffer, and VC71 stages. A direct 20-seed differential passes but reaches only 16.1% of the original body, so the ingame stateful paths and live network behavior remain unverified.

The postgame client idle path now follows PAL's single-result flow through the connectivity check, connection idle, message processing, and final active-connection check. The 2276 binary pushes `network_connection_idle() failed in network_game_client_idle_postgame()` on idle failure; the former C used the server-reliable-endpoint name. VC71 mnemonic match rises from 87.8% to 95.2% and operand match from 76.0% to 95.2%, with no neighboring score or warning regression. Relocation-aware aligned bytes rise from 75/121 (provisional 62.0-71.9%) to 89/126 (70.6-80.2%); this does not establish literal linked-byte identity. The lift pipeline passes ABI, build, hazard, buffer, and VC71 stages. The existing 100-seed differential passes with 57.3% coverage and one observed return value, leaving failure paths and live postgame networking unverified.

`network_game_server_send_game_data_pregame` matches PAL and 2276 on its 0x434-byte game copy, type-6 message creation, broadcast, return value, and error strings. Its local message uses the verified `network_game_blob_t` layout; VC71 is 100% for both mnemonic and operands, and the whole server-source regression gate passes. The default synthetic oracle stubs `network_game_server_get_game` to zero and covers only 28.7% of the function. Supplying the verified `server + 8` game pointer reaches the message-creation failure branch (52.1% coverage); overriding creation and broadcast returns also exercises broadcast success (53.7%) and failure (60.6%) branches. A tracked [`check_system_link_game_data.py`](../tools/equivalence/check_system_link_game_data.py) snapshot supplies a distinctive 0x434-byte game image: both sides pass every byte unchanged to type-6 message creation and call the broadcast helper with the same arguments. This evidence supports enabling the `0x12f5d0` redirect; message creation, network delivery, and live system-link behavior remain unverified.

The ingame add-player handler uses `network_player_record_t` for its 0x20-byte decoded player, matching PAL and the 2276 `EBP-0x28` stack slot. Ghidra confirms the original's ingame state check, type-0x1a/version-1 decode, queue-helper call, and true return on every branch. VC71 remains 100% mnemonic and operand match. The default differential gives 96 pass, three null-server assert divergences, and one unmapped read. Focused accepted-decode and decode-failure snapshots agree at 71.9% oracle coverage. The tracked [`check_system_link_add_player_ingame.py`](../tools/equivalence/check_system_link_add_player_ingame.py) probe confirms that the candidate copies the exact decoded 0x20-byte record to server `+0x498`; a separate standalone queue-helper differential agrees with the original on its copy call arguments at 68.9% coverage. The candidate inlines the queue helper while the outer oracle run intercepts it, so these are compositional checks rather than a full side-effect comparison. This evidence supports enabling `0x12f200`; live queue processing and network delivery remain unverified.

The postgame remove-player handler matches PAL and the 2276 decompile for the postgame state check, type-0x20/version-1 decode, removal-helper call, and true return on every branch. Its VC71 mnemonic and operand scores are 100%. The default differential reports 96 pass, three null-server divergences, and one error; the lift pipeline's PASS is not a blanket equivalence result. The tracked [`check_system_link_remove_player_postgame.py`](../tools/equivalence/check_system_link_remove_player_postgame.py) checks accepted decode at 67.3% outer-handler coverage, decode failure, wrong state, and the active removal helper with a matching machine identifier at 39.3% coverage. A code hook at both removal calls confirms the same server, machine, and exact decoded 0x20-byte player record. This supports enabling `0x12f290`; downstream game-state mutation and live network behavior remain unverified.

`network_game_server_handle_message_client_loaded` matches PAL's pregame state check, type-0x18 decode, loading-complete call, and failure returns. VC71 is 99.1% mnemonic and 88.3% operand match. The default synthetic differential still reports 51 pass, 48 divergence, and one error on invalid server inputs; the lift pipeline reports PASS under its high-match policy, not an equivalence pass. Synthetic pregame success and decode-failure snapshots agree with the 2276 outer handler at 71.2% coverage, and a wrong-state snapshot also agrees. The tracked [`check_system_link_client_loaded.py`](../tools/equivalence/check_system_link_client_loaded.py) probe confirms both sides call the loading-complete helper with the same server and machine pointers. This supports enabling `0x12f170`; the helper is intercepted in that probe, so server-owned machine state and the live loading transition remain unverified.

The game-start request handler's decoded 32-bit word is named `countdown_time`, as in PAL; the 2276 call at `0x12f08d` passes its low 16 bits to `network_game_server_update_countdown`. VC71 remains 100% mnemonic and operand match. Default synthetic seeds still report 51 pass, 48 divergence, and one error when the oracle stubs the server-state getter while the candidate reads invalid synthetic servers; the lift pipeline reports PASS under its 100%-match policy, not an equivalence pass. Valid pregame and wrong-state snapshots agree, as does the decode-failure path. In the tracked [`check_system_link_game_start.py`](../tools/equivalence/check_system_link_game_start.py) snapshot, the oracle's decoder stub and the candidate's inlined decoder both write a decoded value of `2`; both then call the countdown updater with server `0x700000` and value `2`. The pregame success path reaches 71.6% of the oracle handler. This evidence supports enabling `0x12f040`; the countdown helper is intercepted here, so its full state transition and live system-link behavior remain unverified.

The pregame handler uses recovered `network_game_server_game.minimum_players` (+0x10d) and `player_count` (+0x224), the countdown fields, and PAL's connected/loaded flag-bit names. The 2276 disassembly and `network_game_blob_t` layout confirm the offsets. VC71 now reaches 94.4% mnemonic and 93.5% operand match. The ordinary synthetic differential reaches only 25.7% coverage (138 pass, 0 diverge, 2 errors); the active pregame redirect has the 14-path focused verification described above.

The shared `network_game_blob_t` now also names byte `+0x10d` `minimum_players`. PAL tests `server->game.player_count >= server->game.minimum_players`, and the 2276 `network_game_invalidate` body writes 2 to `+0x10d` beside the maximum-player byte at `+0x10e`. This field rename leaves `network_game_invalidate` at 100.0% mnemonic and 98.0% operand match; its lift pipeline passes ABI, build, hazard, buffer, and VC71 gates. The whole server-source VC71 gate passes. The game-manager gate still reports three baselined functions absent from the current source and HEAD, outside this field rename.

The join-game request handler is active. Its address local was enlarged to 0x18 bytes because `network_connection_get_address` can clear that many bytes (2276 `0x12840e`/`0x12843c`). PAL and the 2276 decompile agree on pregame gating, 0x50-byte request decoding, token and hosts-file checks, machine acceptance, and accepted or rejected responses. The machine-index output local is now 32 bits, matching PAL and the callee's 32-bit writes; the former 16-bit local was passed through an `int *` cast. Putting the index assignment before the random-seed call and nesting the pregame body under the state check also follow PAL. Relocation-aware aligned bytes improve from 422/996 to 466/951. VC71 mnemonic match rises from 79.1% to 79.9%, while operand-normalized match falls from 61.2% to 59.2%; the raw alignment is provisional and does not establish literal byte identity. The tracked [`check_system_link_join_request.py`](../tools/equivalence/check_system_link_join_request.py) runner uses a valid server, client-machine pointer, and decoded packet to compare 14 focused paths; all 14 were rerun after this change. It checks exact accepted and rejected packet bytes, connection-write size and flags, host-file decisions, acceptance and game-data calls, event text, return byte, and full server and machine state. The candidate inlines the packet-decoder wrapper; the fixture models its inner `data_packet_group_decode_packet` call, and the probe accounts for the inner error event absent when the oracle wrapper is intercepted. The default pipeline differential still fails on invalid synthetic states (53 pass, 46 diverge, 1 error), so the focused probe is used as the pipeline behavior gate. Two early-return cases fall below the generic differential's coverage floor but pass explicit call and state checks. Decoder, token comparison, file I/O, connection write, and acceptance helpers are intercepted; live join behavior remains unverified.

The PAL 2342 `bungie_net/common/message_header.c` does not contain the 2276 key agreement, TEA, or message encryption implementations; its retained file covers only header construction, byte swapping, and message creation. For 2276 crypto, the binary is the reference. The active `tea_encrypt` and `tea_decrypt` lifts previously used signed 32-bit state and sum operations; Ghidra shows logical right shifts and wrapping arithmetic. Using unsigned state words and explicit wrapping for the sum makes each helper pass 100/100 differential seeds and the lift pipeline's Z3 all-input equivalence proof. Current VC71 match is 96.4%/92.9% (mnemonic/operand) for encrypt and 96.6%/89.7% for decrypt; the whole `message_header.c` regression gate passes. The wrapper probe now runs the original TEA/XOR helpers from the pristine XBE and checks complete frames against the candidate; its 32 length, direction, and secondary-flag cases pass. The active key-agreement builder passed 100/100 direct seeds, but all returned null because `encode_packet_group` was stubbed; its 61.9% coverage misses message creation. The active sieve matches 100/100 synthetic limits in both 0–4 and 2–1000, plus one production `0xffff` run with a longer emulator timeout; allocator and `qsort` calls remain stubbed. The prior unconstrained sieve differential passed 28/100 seeds while 72 exhausted the instruction budget. These checks do not yet establish end-to-end behavior of the four active redirects.

The 64-bit key-agreement arithmetic has a PAL source counterpart. Its `divide64` intentionally seeds the work register from the denominator and subtracts the numerator, matching the 2276 lift even though the argument names suggest the opposite. Direct differential checks pass 100/100 seeds for add, negate, and multiply. Subtract and divide fail direct scratch comparison because the oracle stubs same-object arithmetic callees while the candidate executes them; PAL source and high VC71 match support the source shape, but the direct tests are not same-context proofs for those two helpers.

The active two-word key-parameter generator `FUN_00081170` now follows PAL `generate_key_parameters`' separate prime draws and two-iteration countdown. The 2276 disassembly at `0x81170` confirms the two `FUN_00080eb0(0xffff)` calls, product plus two, `0xffffff` retry threshold, range calls, and assertions. VC71 mnemonic/operand match rose from 68.8%/49.5% to 72.4%/64.9%; the 15-function `thread_win32.c` regression gate passed. The lift pipeline passed ABI, build, hazard, buffer, and VC71 stages but its equivalence stage did not establish a verdict: native prime-generation callees reached the 1,000,000-instruction cap, while a stubbed run escaped to the cross-object prime helper at `0x80eb0`. The generator remains active as before this source recovery.

# 2. Architecture

This section describes five layers, from the bottom up. It is a map of the
debug-2276 code paths, not a statement that every runtime behavior has been
validated on hardware.

```
main loop
 ├─ network_game_client_start_frame 0x12a2d0
 ├─ network_game_server_start_frame 0x12a210
 └─ in game: network_game_client_end_frame 0x12a500
game_time_update 0xb6020  — lockstep tick advance
```

The frame scheduler invokes the client start frame for client and host modes,
the server start frame for host mode, and the client end frame while in game.

The tracked
[`check_system_link_crypto_composed.py`](../tools/equivalence/check_system_link_crypto_composed.py)
runner adds 32 frame-level checks for the active encrypt/decrypt wrappers:
lengths 2, 3, 9, 10, 11, 17, 18, and 19, each encrypted and decrypted with
header bit 1 clear and set. It
executes the original wrapper with its pristine TEA/XOR callees and compares
both sides' entire 32-byte frame against the 2276-backed composition,
including untouched trailing bytes and the header flag. The probe asserts
that the original reaches the expected helper entry for each frame length.
All 32 checks pass. The separately verified TEA helpers have all-input Z3
proofs against the original; live encrypted traffic remains unverified.

The tracked
[`check_system_link_key_message.py`](../tools/equivalence/check_system_link_key_message.py)
runner reaches the key-agreement builder's success path. Oracle and candidate
both pass the same 24 encoded bytes to `create_message` as type 3, with the
same destination, capacity, and encoded size; both return the destination and
write header flag 2. The packet encoder and message allocator are intercepted
with matched effects. This closes the prior null-only synthetic builder
observation but does not verify live key exchange or the encoder's behavior.

The tracked
[`check_system_link_key_arithmetic.py`](../tools/equivalence/check_system_link_key_arithmetic.py)
runner checks three concrete subtract and three divide cases on the active
64-bit key helpers. It confirms modular subtraction and the 2276 divide
operand reversal, including quotient and remainder outputs. All six pass.
These are candidate composition checks against the binary-backed PAL source;
same-context original-versus-candidate arithmetic remains unproven because
the direct harness stubs same-object callees on the original side.

The repeatable
[`check_system_link_indirect_calls.py`](../tools/audit/check_system_link_indirect_calls.py)
audit scans the pristine 2276 bytes for all 391 scoped functions. It finds 35
indirect control transfers: 29 computed jump-table branches whose initial
entries stay within their owning function, and six true indirect calls. Five
of the calls are generic hash-table or LRA-cache callbacks; the pristine
direct-call graph shows no callers of their constructors, but an indirect
caller could still reach them. The sixth is the rejected-client callback in
`network_connection_idle_server_reliable_endpoint` at `0x129c07`. Server
creation installs `network_game_server_reject_connection_game_is_full`
(`0x12db30`, active) at `0x12ef7b`-`0x12ef81`; the rejection call pushes the
accepted endpoint and cleans it alongside the destroy call. This closes the
known indirect call boundary, subject to static disassembly and function
bound limits. Live callback behavior remains unverified.

## Layer 1 — transport / winsock (`src/halo/bungie_net/`, Xbox XNET below)

The current lift consolidates the original endpoint-set and endpoint-winsock
logic in `transport_endpoint_set_winsock.c`; it is the sole registered source
file for `transport_endpoint_set_winsock.obj`.
- An endpoint set is a socket file-descriptor array, with operations `create`, `delete`, `add`, `remove`, `rewind`, `count`, `poll`, and `get_next`.
- The endpoint pool has a cleanup function, `endpoint_pool_cleanup`. Endpoints support create, bind, connect, listen, and accept operations.
- The API includes `recv_endpoint` (0x82e50), `send_endpoint` (0x82f50), `close_endpoint` (0x84000), and the datagram functions `FUN_00084520` (recvfrom) and `FUN_00084740` (sendto). These are active redirects in the current transport lift.
- `game_initialize` calls `transport_initialize` (0x82130) (`src/halo/game/game.c:119`), and `game_dispose` calls `transport_dispose` (0x822d0) (`src/halo/game/game.c:166`). Three helper functions, `transport_get_nonce`, `transport_get_key`, and `transport_get_xnaddr`, support Xbox secure-address authentication.

## Layer 2 — connection (`network_connection.c`, 18/18 ported)

The `network_connection` struct is 0x38 bytes (`src/halo/networking/network_connection.h`). It holds a reliable endpoint with a reliable circular queue, an unreliable endpoint with an unreliable queue, flags, and counters. The server variant, `network_server_connection`, is 0x50 bytes. It adds an endpoint set, four child clients, and an accept gate.

- `network_connection_new(flags, well_known_port)` (0x1296b0) creates a TCP reliable endpoint, a UDP unreliable endpoint, and their queues; the port is a caller-supplied argument, not derived from `flags`. Flag value 1 selects server mode (listen-set limit 5); its caller (`network_server_manager.c:2653`) passes port `0x141e` = 5150. Flag value 2 selects client mode; its caller (`network_client_manager.c:2589`) passes port `0x141f` = 5151.
- `network_connection_connect` (0x128460) runs on the client, asynchronously. `network_connection_server_accept_client_connection` (0x1285c0) runs on the server.
- `network_connection_write` (0x128e00) selects the reliable stream or datagram path from the connection role and requested reliability. Its reliable path handles partial `send_endpoint` writes and treats `-4` as the retryable result; its datagram path uses `FUN_00084740`.
- `network_connection_idle_server_reliable_endpoint` (0x129a30) runs on the server. It calls `poll_endpoint_set`, rewinds the set, then handles each ready endpoint. For a new client, it calls `FUN_00084450` and `network_connection_create_client_from_endpoint` (0x129270). For an existing client, it calls `network_connection_idle_client_reliable_endpoint` (0x1294d0), which receives data into a 0x8000-byte queue. The literal name `network_connection_idle` now belongs to a different, more general timeout/servicing function at 0x129cf0 — defined in `network_game_globals.c` despite an embedded `__FILE__` string that still points at `network_connection.c`.
- The read functions are `network_client_reliable_connection_read` (0x1292f0) and `network_client_unreliable_connection_read` (0x1286e0).

## Layer 3 — message frame + crypto (`message_header.c`)

The wire header is a 16-bit word. Bits 4-15 hold the size (`size = header >> 4`). Bits 0-1 hold flags. Bits 2-3 hold the category (see `GET_MESSAGE_SIZE` and the category mask in the handlers). `build_message_header` (0x80b40) builds this header. `create_message` (0x80ca0) wraps a payload with the header. `byte_swap_message_header` (0x80c20) converts between host and network byte order.

Cryptography functions include `tea_encrypt`/`tea_decrypt` (the TEA cipher), `key_message_xor_keystream`, `message_encrypt`/`message_decrypt` (0x80940/0x80a40), and prime and RSA helpers starting at `FUN_00080eb0`. The key-exchange handlers at 0x805a0 and 0x80620 are active and currently live in `message_header.c`; the four functions discussed in section 1 also have active redirects, though their behavior evidence is incomplete. `network_client_reliable_connection_read` asserts that its received header has no encryption flag. That establishes plaintext as the expected reliable-message form, but is not by itself proof about every packet path.

## Layer 4 — packet serialization (`network_messages.c` + `data_packet_groups.c`)

- `data_packet_groups.c` defines field descriptors as five `short` values, ending with a type-9 terminator. It contains `encode_packet_group` (0x11aca0) and `compute_packet_field_sizes` (0x11add0).
- `network_messages.c` contains decode-state helpers, `data_packet_group_initialize` (0x11a930), `data_packet_group_decode_packet` (0x11aa40), a hash table, and an LRA cache. `initialize_network_game_packets` (0x12b640) validates `s_network_game_messages_group` (0x323510).
- `create_network_game_message` (0x12b700, at `network_messages.c:1422`) checks `message_struct_size` for each of the 35 message types. The source defines, for example, `server_game_advertise` (0x114), `server_game_settings_update` (0x434), `server_game_update` (0x210), and `client_game_update` (0x88). It wraps the message with `create_message`.

## Layer 5 — game protocol / state machines

Key globals: the client pointer at `0x46e8c0`, the server pointer at `0x46e8bc`, the abort flag at `0x46e8c6`, and the keepalive timestamp at `0x46e8c8`. The server static block sits at `0x5a90e0`. The client static block sits at `0x5a95a0`. Two functions in `network_game_globals.c`, `network_game_set_number_of_games_played` and `network_game_set_random_seed`, reuse a local variable named `server` to hold the client pointer. This is a naming inconsistency in the source, not a logic error.

### Client
`network_game_client_idle` (0x127070) dispatches on the state field at `client+0xca6`:

| State       | Handler                                       | Role                           |
|-------------|-----------------------------------------------|--------------------------------|
| 0 searching | `network_game_client_idle_searching` 0x1268a0 | broadcast game search + ping   |
| 1 joining   | `network_game_client_idle_joining` 0x126b60   | TCP connect, send join request |
| 2 pregame   | `network_game_client_idle_pregame` 0x126ce0   | keepalive, settings            |
| 3 ingame    | `network_game_client_idle_ingame` 0x126db0    | stale detection, game updates  |
| 4 postgame  | `network_game_client_idle_postgame` 0x126f40  | keepalive/reconnect            |

For incoming messages, `network_game_client_process_incoming_messages` (0x1260c0) drains the queue through `network_connection_read` (0x1298f0), then `network_game_client_handle_message` (0x127ea0) switches on message type.

### Server
`network_game_server_idle` (0x12eb20) runs the server tick in this order:
1. `network_connection_idle` (0x129cf0) accepts new connections — it calls into `network_connection_idle_server_reliable_endpoint` (0x129a30).
2. `network_game_server_add_new_client` (0x12d880) adds a new client.
3. `network_game_server_handle_public_endpoint` (0x12d9f0) handles public-endpoint datagrams.
4. `network_game_server_handle_client_machines` (0x12e580) handles client machines.
5. The tick dispatches on the state field at `server+4`: state 0 (pregame) calls `network_game_server_idle_pregame_tasks` (0x12e750), state 2 (postgame) calls `network_game_server_idle_postgame_tasks` (0x12db60). State 1 (ingame) has no separate handler listed here.

Datagrams route through `network_game_server_handle_datagram` (0x130270). Connected messages route through `network_game_server_handle_client_message` (0x130580). The broadcast helper `network_game_server_send_message_to_all_machines` (0x12f430) loops over the four machine slots.

Server states: 0 pregame, 1 ingame, 2 postgame.

### Message types
Source: `network_client_message_handler.c:462` and `network_server_message_handler.c:432`.

Client → server:

| Opcode | Name                                    | Notes                                                                                                                         |
|--------|-----------------------------------------|-------------------------------------------------------------------------------------------------------------------------------|
| `0x00` | broadcast_game_search                   | UDP                                                                                                                           |
| `0x01` | ping                                    | UDP                                                                                                                           |
| `0x0c` | join_request                            |                                                                                                                               |
| `0x0d` | add_player_pregame                      |                                                                                                                               |
| `0x0e` | remove_player_pregame                   |                                                                                                                               |
| `0x0f` | settings                                |                                                                                                                               |
| `0x10` | player_settings                         |                                                                                                                               |
| `0x11` | game_start                              |                                                                                                                               |
| `0x12` | graceful_exit_pregame                   |                                                                                                                               |
| `0x13` | message_client_map_is_precached_pregame | The server handler's log string for this case reuses the `0x12` text. This looks like a decompile artifact and is unresolved. |
| `0x18` | loaded                                  |                                                                                                                               |
| `0x19` | client_game_update                      | UDP, every 16 ms                                                                                                              |
| `0x1a` | add_player_ingame                       |                                                                                                                               |
| `0x1b` | remove_player_ingame                    |                                                                                                                               |
| `0x1c` | host_crashed_cry_for_help               |                                                                                                                               |
| `0x1d` | join_new_host                           |                                                                                                                               |
| `0x20` | remove_player_postgame                  |                                                                                                                               |
| `0x21` | switch_to_pregame                       |                                                                                                                               |
| `0x22` | graceful_exit_postgame                  |                                                                                                                               |

Server → client:

| Opcode | Name                   | Notes      |
|--------|------------------------|------------|
| `0x02` | game_advertise         |            |
| `0x03` | pong                   |            |
| `0x04` | machine_accepted       |            |
| `0x05` | machine_rejected       |            |
| `0x06` | game_settings_update   | size 0x434 |
| `0x07` | pregame_countdown      |            |
| `0x08` | begin_game             |            |
| `0x09` | graceful_exit_pregame  |            |
| `0x0a` | pregame_keep_alive     |            |
| `0x0b` | postgame_keep_alive    |            |
| `0x14` | game_update            |            |
| `0x15` | add_player_ingame      |            |
| `0x16` | remove_player_ingame   |            |
| `0x17` | game_over              |            |
| `0x1e` | switch_to_pregame      |            |
| `0x1f` | graceful_exit_postgame |            |

### State sync (`network_game_manager.c`, 20/20 ported)

The `network_game` blob starts at `game+8` and is 0x434 bytes. It holds four machine records (offset `+0x114`, stride 0x44), sixteen player records (offset `+0x226`, stride 0x20), a random seed, and a games-played counter. Functions exist to add, update, and remove machines and players, plus `network_game_spawn_player` and `network_game_reset_for_next_round`. Serialization uses two message types: `message_server_game_settings_update` (0x434 bytes) and `message_server_game_update` (0x210 bytes).

### Determinism / lockstep (`game_time.c:257`)

`game_time_update` handles the case `game_connection() == 2` (server). It calls `network_game_server_get_oldest_client_update_received` to gate `maximum_ticks` against `game_time_get()`. If the server gets more than 0x80 ticks ahead of a client, it asserts: "update server is too far ahead of a client for the client to ever catch up!". `network_game_server_stalled_on_client` toggles the stall state. On the client, state 1 clamps `maximum_ticks` to `update_get_maximum_actions()`. `network_game_server_update_ticks` applies the tick count. This logic is the part most sensitive to desync.

## Join / session trace (host + client)

```
CLIENT idle_searching (0x1268a0)
  encode 0x00 broadcast_game_search -> network_connection_write(UDP, 255.255.255.255:0x141e)
HOST   network_game_server_idle -> network_game_server_handle_public_endpoint -> network_game_server_handle_datagram datagram case 0
        -> handle_message_client_broadcast_game_search (builds 0x114 advertise)
CLIENT FUN_00127260 (0x02 advertise) -> FUN_00125ce0 advertised-games list -> UI
CLIENT initiate_join_game -> network_connection_connect (TCP :0x141e)
HOST   network_connection_idle accept -> network_connection_create_client_from_endpoint
        -> network_game_server_add_new_client add client
CLIENT send 0x0c join_game_request (network_game_client_idle_joining)
HOST   network_game_server_handle_client_message case 0xc -> network_game_server_handle_message_client_join_game_request join handler
        -> network_game_server_accept_client_machine_into_game
HOST   broadcast 0x04 machine_accepted -> 0x06 game_settings_update (network_game_server_send_game_data_pregame)
CLIENT 0x04 -> pregame state 2; HOST state 0 (network_game_server_idle_pregame_tasks loop)
... countdown 0x07 / begin_game 0x08 -> state 3 ...
INGAME: CLIENT end_frame -> 0x19 client_game_update (UDP) every 16ms
        HOST network_game_server_handle_datagram case 0x19 -> handle_client_update_packet
        HOST broadcast 0x14 server_game_update -> CLIENT FUN_00127a50
```

## Practical notes

- PAL 2342 has typed source for the server idle path and ping handler. The 15-object 2276 pipeline now has complete redirect coverage in the local patched build; runtime verification remains. PAL is a source-shape reference, subject to 2276 binary checks: PAL assigns the return of `network_server_close_client_connection` in the rejected-client path, whereas 2276 at `0x12ebe1` ignores it. The server idle VC71 mnemonic match is now 98.6% with no neighboring regression. The server state values 0/1/2 and their pregame/ingame/postgame names are confirmed by the 2276 switch and PAL handlers.

- `docs/system-link-rng-desync.md` is the authoritative record for the open RNG-desync investigation. Its retained captures show RNG divergence downstream of earlier collision, matrix, or float-order differences in the tested cases; they do not establish a universal initiating cause. Session ds94 implicated `FUN_001a2f40`, which remains `ported:false`. Bisection is incomplete.
- `docs/networking_system_link_bug.md` is a historical regression note. Its function inventory is obsolete: it predates the active transport lift and `0x12e1d0` is now the active `network_game_server_stalled_on_client` redirect. Its pregame-call theory remains unverified and must not be treated as a current root-cause finding.
- In the debug binary, transport error codes map to strings at `0x81c80` and to an enum table at `0x266170`.
- `kb.json` object grouping is useful for coverage bookkeeping, not a proof of subsystem ownership. Use call paths and source evidence before treating an object name as an architectural boundary.
