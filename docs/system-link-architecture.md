# System Link Architecture

This document describes the networking layers and session flow in Halo Xbox
debug build `01.10.12.2276` (October 12, 2001). Addresses are virtual addresses
in `halo-patched/cachebeta.xbe`, MD5 `c7869590a1c64ad034e49a5ee0c02465`.

Function names refer to the recovered implementations in this repository.
The supporting records are [`kb.json`](../kb.json), the
[2276 function bounds](../tools/verify/function_bounds.json), and the layout
evidence linked below. Implementation scores, redirect status, and test-run
history are outside this architecture description.

## Transport

Source: [transport_endpoint_set_winsock.c](../src/halo/bungie_net/network/transport_endpoint_set_winsock.c).

`transport_initialize` (`0x82130`) and `transport_dispose` (`0x822d0`)
manage transport lifetime. Endpoints provide socket creation, bind, connect,
listen, accept, send, receive, and close operations. Endpoint sets provide
add, remove, rewind, poll, count, and iteration operations.

| Operation | Function address |
|-----------|------------------|
| Stream receive | `recv_endpoint`, `0x82e50` |
| Stream send | `send_endpoint`, `0x82f50` |
| Socket creation | `0x83930` |
| Bind | `0x83e20` |
| Close | `close_endpoint`, `0x84000` |
| Asynchronous connect | `0x841b0` |
| Listen | `0x843a0` |
| Accept | `0x84450` |
| Datagram receive | `0x84520` |
| Datagram send | `0x84740` |

`transport_get_nonce`, `transport_get_key`, and `transport_get_xnaddr`
provide the nonce, key, and Xbox network address used by the game-search path.

## Connections

Source: [network_connection.c](../src/halo/networking/network_connection.c)
and [network_connection.h](../src/halo/networking/network_connection.h).
Layout evidence: [network_connection.json](../recovery/evidence/network_connection.json).

A `network_connection` occupies `0x38` bytes. It holds reliable and unreliable
endpoints, their incoming circular queues, keepalive time, a rejection
callback, traffic counters, flags, and a well-known port. The `0x50`-byte
server connection extends it with an endpoint set, four child-connection
slots, and an accept gate.

`network_connection_new` (`0x1296b0`) creates the reliable TCP endpoint,
unreliable UDP endpoint, and queues. The well-known port is a separate
argument from the flags. Server creation uses flags `1` and port `0x141e`
(5150); client creation uses flags `2` and port `0x141f` (5151).

- `network_connection_connect` (`0x128460`) starts the client's asynchronous
  connection. `network_connection_server_accept_client_connection`
  (`0x1285c0`) accepts it on the server.
- `network_connection_write` (`0x128e00`) selects the stream or datagram path
  according to connection role and requested reliability.
- `network_connection_idle_server_reliable_endpoint` (`0x129a30`) polls the
  endpoint set. New endpoints pass through the accept function (`0x84450`)
  and `network_connection_create_client_from_endpoint` (`0x129270`). Existing
  connections are serviced by
  `network_connection_idle_client_reliable_endpoint` (`0x1294d0`).
- `network_client_reliable_connection_read` (`0x1292f0`) and
  `network_client_unreliable_connection_read` (`0x1286e0`) read queued frames.
  `network_connection_read` (`0x1298f0`) provides the common read entry.
- `network_connection_idle` (`0x129cf0`) services connections and timeouts.

## Message framing and encryption

Source: [message_header.c](../src/halo/networking/message_header.c).

`build_message_header` (`0x80b40`) constructs a 16-bit header:

| Bits | Meaning |
|------|---------|
| 0–1 | Flags |
| 2–3 | Message category |
| 4–15 | Total frame length in bytes, including the two-byte header |

The length is `header >> 4`, with a maximum of `0xfff`.
`create_message` (`0x80ca0`) adds two bytes to the payload length and copies
the payload after the header. `byte_swap_message_header` (`0x80c20`) swaps
the header bytes for transmission or reception.

The message categories are `1` (low-level error), `2` (data), and `3`
(game message). The game-message handlers dispatch on the opcode byte at
`message[message_size - 1]`.

The encryption helpers are `tea_encrypt` (`0x80820`), `tea_decrypt`
(`0x808b0`), and `key_message_xor_keystream`. `message_encrypt` (`0x80940`)
and `message_decrypt` (`0x80a40`) apply TEA to complete eight-byte payload
blocks and the XOR helper to remaining bytes. Header bit 0 records encryption.
The reliable connection reader asserts `"encryption should not be active"`
when consuming a frame.

## Packet serialization

Sources: [data_packet_groups.c](../src/halo/networking/data_packet_groups.c)
and [network_messages.c](../src/halo/networking/network_messages.c).

`data_packet_group_initialize` (`0x11a930`),
`data_packet_group_decode_packet` (`0x11aa40`), and
`encode_packet_group` (`0x11aca0`) handle descriptor-based serialization.
`initialize_network_game_packets` (`0x12b640`) initializes the network-game
packet group at `0x323510`.

`create_network_game_message` (`0x12b700`) checks the supplied decoded
structure size for each of the 35 opcodes (`0x00` through `0x22`), encodes
it, and wraps the encoded data with `create_message`. These are decoded
structure sizes, rather than encoded frame lengths:

| Message | Structure size |
|---------|----------------|
| Server game advertisement | `0x114` |
| Server game settings update | `0x434` |
| Server game update | `0x210` |
| Client game update | `0x88` |

## Client and server state machines

Sources: [network_client_manager.c](../src/halo/networking/network_client_manager.c),
[network_server_manager.c](../src/halo/networking/network_server_manager.c),
[network_client_message_handler.c](../src/halo/networking/network_client_message_handler.c),
and [network_server_message_handler.c](../src/halo/networking/network_server_message_handler.c).

### Client

`network_game_client_idle` (`0x127070`) reads the 16-bit state at
`client + 0xca6` and dispatches to the corresponding handler:

| State | Handler | Address |
|-------|---------|---------|
| 0 searching | `network_game_client_idle_searching` | `0x1268a0` |
| 1 joining | `network_game_client_idle_joining` | `0x126b60` |
| 2 pregame | `network_game_client_idle_pregame` | `0x126ce0` |
| 3 ingame | `network_game_client_idle_ingame` | `0x126db0` |
| 4 postgame | `network_game_client_idle_postgame` | `0x126f40` |

`network_game_client_process_incoming_messages` (`0x1260c0`) drains received
messages through `network_connection_read`, then passes each frame to
`network_game_client_handle_message` (`0x127ea0`).

### Server

`network_game_server_idle` (`0x12eb20`) checks transport availability and
game validity, then services the server in this order:

1. `network_connection_idle` (`0x129cf0`) services the connection and returns
   any new connection.
2. If a new connection exists, `network_game_server_add_new_client`
   (`0x12d880`) attempts to add it. A rejected connection is closed.
3. `network_game_server_handle_public_endpoint` (`0x12d9f0`) processes
   public-endpoint datagrams.
4. `network_game_server_handle_client_machines` (`0x12e580`) processes
   connected client machines.
5. The 16-bit state at `server + 4` selects the state-specific work.

| State | Idle work |
|-------|-----------|
| 0 pregame | `network_game_server_idle_pregame_tasks`, `0x12e750` |
| 1 ingame | No additional state-specific call in this dispatcher |
| 2 postgame | `network_game_server_idle_postgame_tasks`, `0x12db60` |

Datagrams route through `network_game_server_handle_datagram` (`0x130270`).
Connected messages route through `network_game_server_handle_client_message`
(`0x130580`). `network_game_server_send_message_to_all_machines` (`0x12f430`)
iterates the four server machine slots.

### Game message opcodes

Client to server:

| Opcode | Message |
|--------|---------|
| `0x00` | broadcast_game_search |
| `0x01` | ping |
| `0x0c` | join_request |
| `0x0d` | add_player_pregame |
| `0x0e` | remove_player_pregame |
| `0x0f` | settings |
| `0x10` | player_settings |
| `0x11` | game_start |
| `0x12` | graceful_exit_pregame |
| `0x13` | map_is_precached_pregame |
| `0x18` | loaded |
| `0x19` | client_game_update |
| `0x1a` | add_player_ingame |
| `0x1b` | remove_player_ingame |
| `0x1c` | host_crashed_cry_for_help |
| `0x1d` | join_new_host |
| `0x20` | remove_player_postgame |
| `0x21` | switch_to_pregame |
| `0x22` | graceful_exit_postgame |

Server to client:

| Opcode | Message |
|--------|---------|
| `0x02` | game_advertise |
| `0x03` | pong |
| `0x04` | machine_accepted |
| `0x05` | machine_rejected |
| `0x06` | game_settings_update |
| `0x07` | pregame_countdown |
| `0x08` | begin_game |
| `0x09` | graceful_exit_pregame |
| `0x0a` | pregame_keep_alive |
| `0x0b` | postgame_keep_alive |
| `0x14` | game_update |
| `0x15` | add_player_ingame |
| `0x16` | remove_player_ingame |
| `0x17` | game_over |
| `0x1e` | switch_to_pregame |
| `0x1f` | graceful_exit_postgame |

## Shared game state and tick synchronization

Sources: [network_game_manager.c](../src/halo/networking/network_game_manager.c)
and [game_time.c](../src/halo/game/game_time.c).
Layout evidence: [network_game_blob.json](../recovery/evidence/network_game_blob.json),
[network_machine_record.json](../recovery/evidence/network_machine_record.json),
and [network_player_record.json](../recovery/evidence/network_player_record.json).

The shared game data begins at `game + 8` and occupies `0x434` bytes.
Offsets below are relative to that data:

| Offset | Data |
|--------|------|
| `+0x114` | Four machine records, each `0x44` bytes |
| `+0x226` | Sixteen player records, each `0x20` bytes |
| `+0x428` | Random seed |
| `+0x42c` | Number of games played |

The live server connection slots are a separate array at `server + 0x43c`,
with four `0x10`-byte entries. Their layout is recorded in
[network_server_machine_slot.json](../recovery/evidence/network_server_machine_slot.json).

`game_time_update` (`0xb6020`) limits simulation progress according to the
connection mode. In server mode (`game_connection() == 2`), it reads
`network_game_server_get_oldest_client_update_received` and compares that
value with `game_time_get()`. A lead greater than `0x80` ticks triggers the
assertion `"update server is too far ahead of a client for the client to ever catch up!"`.
It adjusts the tick limit and calls `network_game_server_stalled_on_client`
to set or clear the stall state. Server tick advancement is passed to
`network_game_server_update_ticks` (`0x12cdb0`). In client mode
(`game_connection() == 1`), the available actions from
`update_client_get_maximum_actions` constrain the tick count.

## Discovery and session flow

1. The searching client sends opcode `0x00` by UDP to
   `255.255.255.255:5150`. The server's public-endpoint handler passes it to
   `network_game_server_handle_datagram`, which dispatches to the game-search
   handler (`0x12f690`).
2. The server responds with opcode `0x02`, a game advertisement. The client
   advertisement handler (`0x127260`) updates the advertised-games list
   through `add_advertised_game` (`0x125ce0`).
3. `network_game_client_initiate_join_game` (`0x124aa0`) starts a TCP
   connection to the server. The server accepts the endpoint and adds the
   client connection.
4. The joining client sends opcode `0x0c`. The server's connected-message
   dispatcher calls the join-request handler (`0x12f990`), which accepts or
   rejects the machine. The responses are `0x04` (accepted) or `0x05`
   (rejected); game settings are sent as `0x06`.
5. The accepted client enters state `2` (pregame), while the server is in
   state `0` (pregame). Countdown and begin-game messages use `0x07` and
   `0x08`.
6. During the game, client updates use `0x19` and server updates use `0x14`.
   The server datagram handler processes client updates; the client handler
   at `0x127a50` processes server updates.
