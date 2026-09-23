# System Link Architecture

Sources: current `kb.json` function inventory, the networking source under
`src/halo/networking/` and `src/halo/bungie_net/`,
`artifacts/ntsc_callgraph/callgraph.json`, and
`docs/system-link-rng-desync.md`. The port-status snapshot below was checked
on 2026-09-22. Historical investigation notes are useful evidence, but do not
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
| `network_server_manager.obj`         |            70/84 |                   14 | The deactivated server-state routines are listed below.                                                                                                              |

## 1a. Implemented but deactivated (`ported:false`)

The 14 entries listed here are all in `network_server_manager.obj`. Their
source definitions are in `src/halo/networking/network_server_manager.c`, which
is registered in `src/CMakeLists.txt`. Their `ported:false` setting makes a
freshly built patched XBE bypass those C bodies and route calls to original
binary behavior; it does not mean that the C definitions are absent, nor does
it verify the identity of an already deployed XBE. Other objects also contain
deactivated functions and are outside this system-link-focused list. Thirteen
of these 14 retain an allowlist reason that calls them sub-80% VC71 lifts
pending score improvement; that is historical metadata, not current
measurement evidence. The committed VC71 score file has a score for every
function in this table (range: 77.1% to 100.0%). Those scores are mnemonic-LCS
comparisons against XBE-synthesized references, not raw-byte accuracy or
runtime proof.
`0x12f990` is separately allowlisted as a pre-existing deactivation with no
recorded reason. They are a useful, bounded surface for system-link work, not
evidence that they cause a current regression.

| Addr       | Function                                                              | VC71 mnemonic | Role                             |
|------------|------------------------------------------------------------------------|--------------:|----------------------------------|
| `0x12eb20` | `network_game_server_idle`                                            |      98.6% | server main tick; 86.5% raw score |
| `0x12eca0` | `network_game_server_reset_to_pregame`                                |      92.3% | postgame→pregame reset           |
| `0x12e750` | `network_game_server_idle_pregame_tasks`                              |      79.4% | server pregame (state 0) tick    |
| `0x12e580` | `network_game_server_handle_client_machines`                          |      77.5% | handle client machines           |
| `0x12d880` | `network_game_server_add_new_client`                                  |      81.9% | add new client connection        |
| `0x12dc20` | `network_game_server_setup_game_from_playlist`                        |      84.1% | set up variant/name/open game; PAL and 2276 confirm `game_name` at game `+0`, `map_version` at `+0x20`, and `maximum_teams` at `+0x10f`; typed accesses preserve the 84.7%/72.7% mnemonic/operand scores |
| `0x12f5d0` | `network_game_server_send_game_data_pregame`                          |     100.0% | broadcast pregame game data      |
| `0x12f690` | `handle_message_client_broadcast_game_search`                         |      89.4% | advertise game (broadcast reply) |
| `0x12f8d0` | `handle_message_client_ping`                                          |      77.9% | client ping handler              |
| `0x12f990` | `network_game_server_handle_message_client_join_game_request`         |      79.1% | join-game-request handler        |
| `0x12f040` | `network_game_server_handle_message_client_game_start_request`        |     100.0% | client game-start request        |
| `0x12f170` | `network_game_server_handle_message_client_loaded`                    |      99.1% | client loaded                    |
| `0x12f200` | `network_game_server_handle_message_client_add_player_request_ingame` |     100.0% | add player ingame                |
| `0x12f290` | `network_game_server_handle_message_client_remove_player_request_postgame` |     100.0% | remove player postgame           |

`FUN_00083930` (socket creation), `FUN_00083e20` (bind),
`FUN_000841b0` (asynchronous connect), `FUN_000843a0` (listen), and
`FUN_00084740` (datagram send) are now active transport redirects.

For inactive `network_game_server_setup_game_from_playlist`, the recovered `network_game_blob_t` layout now identifies the 16-wide-character game name at `+0`, map version at `+0x20`, and maximum teams at `+0x10f`. PAL names and the 2276 stores agree; typed accesses preserve the 84.7% mnemonic and 72.7% operand match. Synthetic server snapshots reach playlist lookup failure and both team and non-team success branches, with matching outer return and observed memory. On success, the candidate calls `csmemcpy`/`csmemset` for the local default name while the oracle initializes it inline, so call-sequence comparison diverges and those helper effects remain unverified. The redirect stays inactive.

The active ping reply handler previously wrote IPv4 address length at reply-address `+2` and port at `+4`, overwriting source address bytes. The 2276 instructions write length at `+0x10` and port at `+0x12`, matching PAL and the verified `transport_address` layout. The corrected overlapping local uses that type and retains the 80.0% mnemonic/66.2% operand match. The lift pipeline passes 100/100 default synthetic seeds at 56.1% coverage; a synthetic successful send reaches 64.6% but cannot prove the full network write because the helper is stubbed or inlined across the comparison.

PAL's `message_client_ping` is an eight-byte payload with timestamp at `+0`, port at `+4`, and two padding bytes; the 2276 handler uses those widths and offsets. A local checked type now replaces the raw decoded-buffer accesses without changing the ping handler's VC71 scores.

## 1b. Full pipeline inventory (2276)

The 15 scoped `kb.json` objects below contain 391 functions. Of these, 377 have active redirects and 14 server handlers explicitly use `ported:false`; all 14 inactive entries have C bodies. The four message-header entries with no `ported` field in the nested object list have top-level `ported:true` records. The patcher uses those records, and a rebuilt XBE confirms redirects at their original addresses. The earlier 13-object count missed `thread_win32.obj` (thread/mutex and key-agreement helpers) and `64bit_math.obj` (key-agreement arithmetic); both are fully active. The PAL 2342 source provides counterparts for the client/server state machines; its ABI, source shape, and behavior must be checked against the 2276 binary before reuse. Its message-header file lacks the 2276 encryption implementations.

| Pipeline stage | Objects | Active / total |
|---|---|---:|
| Transport | `transport_endpoint_set_winsock.obj`, `transport_address.obj` | 56 / 56 |
| Message framing and crypto | `message_header.obj` | 23 / 23 |
| Key-agreement and platform support | `thread_win32.obj`, `64bit_math.obj` | 20 / 20 |
| Connection | `network_connection.obj` | 18 / 18 |
| Serialization | `data_packet_groups.obj`, `network_messages.obj` | 42 / 42 |
| Client | `network_game_globals.obj`, `network_client_manager.obj`, `network_client_message_handler.obj` | 100 / 100 |
| Server | `network_server_manager.obj`, `network_server_message_handler.obj` | 78 / 92 |
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

PAL review found three distinct cases. The 2276 transport functions `transport_dispose` and `transport_network_available` have low mnemonic percentages because their reference bodies are very short; Ghidra confirms their current behavior, including a `void` return for `transport_dispose` where PAL returns an error code. The client search and server machine handlers have longer low-match bodies: PAL's explicit result handling improves the 2276 client search score, while binary-backed branch and layout recovery improves the server handler's operand match. Finally, four disabled server handlers score 100% in both mnemonic and operand comparison, but a direct differential run of the game-start handler diverges when its candidate inlines `network_game_server_get_state` and reaches an assert while the oracle stubs that callee. That run does not establish a runtime regression or safe activation. Redirects remain disabled pending a same-context oracle or system-link runtime test.

`network_game_server_send_game_data_pregame` is one of the 100% matching disabled handlers. PAL and 2276 agree on its 0x434-byte game copy, type-6 message creation, broadcast, return value, and error strings. Its local message now uses the verified `network_game_blob_t` layout in place of an opaque byte array; VC71 remains 100% for both mnemonic and operands, and the whole server-source regression gate passes. The default synthetic oracle stubs `network_game_server_get_game` to zero and covers only 28.7% of the function. Supplying the verified `server + 8` game pointer as a deterministic stub return reaches the message-creation failure branch (52.1% coverage); overriding creation and broadcast returns also exercises broadcast success (53.7%) and failure (60.6%) branches. Each targeted snapshot agrees on the reached path, but message creation, payload copying, and sending remain stubbed. The redirect stays disabled pending an end-to-end system-link test or a stronger same-context oracle.

The disabled postgame remove-player handler now decodes into the verified 0x20-byte `network_player_record_t` instead of an opaque 32-byte array, matching PAL's `struct network_player` local and the 2276 stack slot at `EBP-0x28`. VC71 remains 100% mnemonic and operand match, with no server-source regression. Its default synthetic differential reaches only 26.9% coverage: 96 seeds pass, three diverge when a null server makes the candidate's same-object state getter assert while the oracle stubs that getter, and one seed has an unmapped candidate read. A synthetic server with postgame state 2 and deterministic decoder/removal returns exercises decode failure (62.2% coverage), removal failure (75.6%), and removal success (67.3%); all three targeted paths agree with the oracle. The decoder and player removal remain stubbed, so these tests do not yet prove the full packet path or justify activation.

The disabled ingame add-player handler likewise uses `network_player_record_t` for its 0x20-byte decoded player, matching PAL and the 2276 `EBP-0x28` stack slot. VC71 remains 100% mnemonic and operand match, and its lift pipeline and server-source gates pass. The default differential gives 96 pass, three null-server assert divergences, and one unmapped read. A synthetic ingame server reaches the decode-failure branch and agrees with the oracle (71.9% coverage). With an accepted decode, the candidate enters its same-object queue helper and calls `csmemcpy` while the oracle stubs the helper, producing a call-sequence mismatch despite agreeing on the outer return. A valid queue and serialized packet path still need verification before activation.

`network_game_server_handle_message_client_loaded` matches PAL's pregame state check, type-0x18 decode, loading-complete call, and failure returns. A fresh VC71 check reports 99.1% mnemonic and 88.3% operand match. Synthetic pregame snapshots with decoder failure and success both agree with the 2276 outer handler and each reach 71.2% coverage, but the loading-complete helper is stubbed on both sides. The server-owned machine state and subsequent loading transition remain untested; the redirect stays disabled.

The disabled game-start request handler's decoded 32-bit word is now named `countdown_time`, as in PAL; the 2276 call at `0x12f08d` passes that local to `network_game_server_update_countdown`. VC71 remains 100% mnemonic and operand match, with no server-source regression, and its lift pipeline passes. Default synthetic seeds give 51 pass, 48 divergence, and one error because the oracle stubs the server-state getter while the candidate can assert on an invalid server. Synthetic pregame snapshots with decoder failure and success each agree on the outer handler (71.6% coverage); the countdown update is stubbed, so a real decoded countdown and state transition remain to be tested before activation.

The pregame handler now uses recovered `network_game_server_game.minimum_players` (+0x10d) and `player_count` (+0x224), the countdown fields, and PAL's connected/loaded flag-bit names. The 2276 disassembly and the existing `network_game_blob_t` layout confirm the offsets. This is a source recovery with unchanged 79.4% mnemonic and 77.2% operand match. The lift pipeline passes ABI, build, and hazards; its synthetic differential reaches only 25.7% coverage (138 pass, 0 diverge, 2 errors), so the pregame redirect remains disabled.

The shared `network_game_blob_t` now also names byte `+0x10d` `minimum_players`. PAL tests `server->game.player_count >= server->game.minimum_players`, and the 2276 `network_game_invalidate` body writes 2 to `+0x10d` beside the maximum-player byte at `+0x10e`. This field rename leaves `network_game_invalidate` at 100.0% mnemonic and 98.0% operand match; its lift pipeline passes ABI, build, hazard, buffer, and VC71 gates. The whole server-source VC71 gate passes. The game-manager gate still reports three baselined functions absent from the current source and HEAD, outside this field rename.

The join-game request handler had a 16-byte local address buffer while `network_connection_get_address` can clear 0x18 bytes at that pointer (2276 `0x12840e`/`0x12843c`). The local is now 0x18 bytes. VC71 remains 79.1% mnemonic and 60.9% operand with no server-object regression. Its lift pipeline passes build and hazard gates but fails synthetic equivalence (53 pass, 46 diverge, 1 error): the candidate inlines server helpers and asserts in states where the oracle stubs those callees. A second snapshot with a non-null pregame server still fails at an inlined assert because it does not model a server-owned client-machine pointer. These tests do not cover a valid join state or justify redirect activation.

The PAL 2342 `bungie_net/common/message_header.c` does not contain the 2276 key agreement, TEA, or message encryption implementations; its retained file covers only header construction, byte swapping, and message creation. For 2276 crypto, the binary is the reference. The active `tea_encrypt` and `tea_decrypt` lifts previously used signed 32-bit state and sum operations; Ghidra shows logical right shifts and wrapping arithmetic. Using unsigned state words and explicit wrapping for the sum makes each helper pass 100/100 differential seeds and the lift pipeline's Z3 all-input equivalence proof. Current VC71 match is 96.4%/92.9% (mnemonic/operand) for encrypt and 96.6%/89.7% for decrypt; the whole `message_header.c` regression gate passes. The active `message_encrypt` and `message_decrypt` wrappers still need same-context verification: the direct synthetic oracle stubs TEA while the compiled candidate executes its same-file TEA body, even for a valid 10-byte frame. Enabling `--oracle-native-callees` with that frame still compared 71 oracle instructions against 867 candidate instructions and found a scratch-buffer difference; the oracle did not run TEA because the candidate inlined it. The active key-agreement builder passed 100/100 direct seeds, but all returned null because `encode_packet_group` was stubbed; its 61.9% coverage misses message creation. The active sieve matches 100/100 synthetic limits in both 0–4 and 2–1000, plus one production `0xffff` run with a longer emulator timeout; allocator and `qsort` calls remain stubbed. The prior unconstrained sieve differential passed 28/100 seeds while 72 exhausted the instruction budget. These checks do not yet establish end-to-end behavior of the four active redirects.

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
`network_game_server_idle` (0x12eb20, deactivated) runs the server tick in this order:
1. `network_connection_idle` (0x129cf0) accepts new connections — it calls into `network_connection_idle_server_reliable_endpoint` (0x129a30).
2. `network_game_server_add_new_client` (0x12d880, deactivated) adds a new client.
3. `network_game_server_handle_public_endpoint` (0x12d9f0) handles public-endpoint datagrams.
4. `network_game_server_handle_client_machines` (0x12e580, deactivated) handles client machines.
5. The tick dispatches on the state field at `server+4`: state 0 (pregame) calls `network_game_server_idle_pregame_tasks` (0x12e750, deactivated), state 2 (postgame) calls `network_game_server_idle_postgame_tasks` (0x12db60). State 1 (ingame) has no separate handler listed here.

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
        -> handle_message_client_broadcast_game_search [deact]  (builds 0x114 advertise)
CLIENT FUN_00127260 (0x02 advertise) -> FUN_00125ce0 advertised-games list -> UI
CLIENT initiate_join_game -> network_connection_connect (TCP :0x141e)
HOST   network_connection_idle accept -> network_connection_create_client_from_endpoint
        -> network_game_server_add_new_client add client [deact]
CLIENT send 0x0c join_game_request (network_game_client_idle_joining)
HOST   network_game_server_handle_client_message case 0xc -> network_game_server_handle_message_client_join_game_request join handler [deact]
        -> network_game_server_accept_client_machine_into_game
HOST   broadcast 0x04 machine_accepted -> 0x06 game_settings_update (network_game_server_send_game_data_pregame)
CLIENT 0x04 -> pregame state 2; HOST state 0 (network_game_server_idle_pregame_tasks loop)
... countdown 0x07 / begin_game 0x08 -> state 3 ...
INGAME: CLIENT end_frame -> 0x19 client_game_update (UDP) every 16ms
        HOST network_game_server_handle_datagram case 0x19 -> handle_client_update_packet
        HOST broadcast 0x14 server_game_update -> CLIENT FUN_00127a50
```

## Practical notes

- PAL 2342 has typed source for the server idle path and ping handler. All inactive functions in section 1 have C bodies in this 2276 tree; the remaining gap is redirect activation and verification. PAL is a source-shape reference, subject to 2276 binary checks: PAL assigns the return of `network_server_close_client_connection` in the rejected-client path, whereas 2276 at `0x12ebe1` ignores it. PAL's explicit `== TRUE` checks improved the local 2276 VC71 match of `network_game_server_idle` from the committed 83.8% to 86.5%, with no neighboring regression. The server state values 0/1/2 and their pregame/ingame/postgame names are confirmed by the 2276 switch and PAL handlers.

- `docs/system-link-rng-desync.md` is the authoritative record for the open RNG-desync investigation. Its retained captures show RNG divergence downstream of earlier collision, matrix, or float-order differences in the tested cases; they do not establish a universal initiating cause. Session ds94 implicated `FUN_001a2f40`, which remains `ported:false`. Bisection is incomplete.
- `docs/networking_system_link_bug.md` is a historical regression note. Its function inventory is obsolete: it predates the active transport lift and `0x12e1d0` is now the active `network_game_server_stalled_on_client` redirect. Its pregame-call theory remains unverified and must not be treated as a current root-cause finding.
- In the debug binary, transport error codes map to strings at `0x81c80` and to an enum table at `0x266170`.
- `kb.json` object grouping is useful for coverage bookkeeping, not a proof of subsystem ownership. Use call paths and source evidence before treating an object name as an architectural boundary.
