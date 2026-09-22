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
| `message_header.obj`                 |            19/23 |                    4 | The inactive functions are `key_agreement_build_message` (0x803d0), `message_encrypt` (0x80940), `message_decrypt` (0x80a40), and `sieve_of_eratosthenes` (0x80d50). |
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
function in this table (range: 71.2% to 100.0%). Those scores are mnemonic-LCS
comparisons against XBE-synthesized references, not raw-byte accuracy or
runtime proof.
`0x12f990` is separately allowlisted as a pre-existing deactivation with no
recorded reason. They are a useful, bounded surface for system-link work, not
evidence that they cause a current regression.

| Addr       | Function                                                              | VC71 mnemonic | Role                             |
|------------|------------------------------------------------------------------------|--------------:|----------------------------------|
| `0x12eb20` | `network_game_server_idle`                                            |      71.2% | server main tick                 |
| `0x12eca0` | `network_game_server_reset_to_pregame`                                |      92.3% | postgame→pregame reset           |
| `0x12e750` | `network_game_server_idle_pregame_tasks`                              |      79.4% | server pregame (state 0) tick    |
| `0x12e580` | `network_game_server_handle_client_machines`                          |      77.1% | handle client machines           |
| `0x12d880` | `network_game_server_add_new_client`                                  |      81.9% | add new client connection        |
| `0x12dc20` | `network_game_server_setup_game_from_playlist`                        |      84.1% | set up variant/name/open game    |
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

Cryptography functions include `tea_encrypt`/`tea_decrypt` (the TEA cipher), `key_message_xor_keystream`, `message_encrypt`/`message_decrypt` (0x80940/0x80a40), and prime and RSA helpers starting at `FUN_00080eb0`. The key-exchange handlers at 0x805a0 and 0x80620 are active and currently live in `message_header.c`; the message-builder and crypto functions listed in section 1 remain on original bytes. `network_client_reliable_connection_read` asserts that its received header has no encryption flag. That establishes plaintext as the expected reliable-message form, but is not by itself proof about every packet path.

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

- `docs/system-link-rng-desync.md` is the authoritative record for the open RNG-desync investigation. Its retained captures show RNG divergence downstream of earlier collision, matrix, or float-order differences in the tested cases; they do not establish a universal initiating cause. Session ds94 implicated `FUN_001a2f40`, which remains `ported:false`. Bisection is incomplete.
- `docs/networking_system_link_bug.md` is a historical regression note. Its function inventory is obsolete: it predates the active transport lift and `0x12e1d0` is now the active `network_game_server_stalled_on_client` redirect. Its pregame-call theory remains unverified and must not be treated as a current root-cause finding.
- In the debug binary, transport error codes map to strings at `0x81c80` and to an enum table at `0x266170`.
- `kb.json` object grouping is useful for coverage bookkeeping, not a proof of subsystem ownership. Use call paths and source evidence before treating an object name as an architectural boundary.
