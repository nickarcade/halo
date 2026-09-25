#ifdef HALO_RNG_TRACE
#include "halo/math/rng_trace.h"
#endif

/* Recovered network game client state enum from assert strings and logs */
enum network_game_client_state {
  _network_game_client_state_searching = 0,
  _network_game_client_state_joining = 1,
  _network_game_client_state_pregame = 2,
  _network_game_client_state_ingame = 3,
  _network_game_client_state_postgame = 4
};

/* Recovered network game globals struct layout at 0x46e8bc.
 * Derived from network_game_globals.c field access patterns and docs/system-link-architecture.md.
 */
typedef struct network_game_globals_t {
  void *server;                           /* 0x00 (0x46e8bc) */
  void *client;                           /* 0x04 (0x46e8c0) */
  bool accept_remote_connections;         /* 0x08 (0x46e8c4) */
  bool quickstart_local;                  /* 0x09 (0x46e8c5) */
  bool abort;                             /* 0x0a (0x46e8c6) */
  uint8_t pad_0b;                         /* 0x0b (0x46e8c7) */
  uint32_t last_update_time;              /* 0x0c (0x46e8c8) */
} network_game_globals_t;
cs(network_game_globals_t, 0x10);
co(network_game_globals_t, server, 0x00);
co(network_game_globals_t, client, 0x04);
co(network_game_globals_t, accept_remote_connections, 0x08);
co(network_game_globals_t, quickstart_local, 0x09);
co(network_game_globals_t, abort, 0x0a);
co(network_game_globals_t, last_update_time, 0x0c);

#define network_game_globals (*(network_game_globals_t *)0x46e8bc)
#define network_connection_dont_timeout (*(bool *)0x46e8ba)
#define s_last_client_state (*(int16_t *)0x322cd8)

#line 1
#include "network_connection.h"
/* network_server_close_client_connection (0x129130)
 * Disconnect a client connection from a server connection's client list.
 * Removes the client's endpoint from the server's endpoint set, disposes the
 * client connection, and clears the slot.
 *
 * NOTE: __FILE__ string references network_connection.c, but this function
 * is linked into network_game_globals.obj.
 */
bool network_server_close_client_connection(int server_connection, int client_connection)
{
  short result;
  network_server_connection *server;
  network_connection *client;
  network_connection **slot;
  int i;

  server = (network_server_connection *)server_connection;
  client = (network_connection *)client_connection;

  if (server == NULL) {
    display_assert("server_connection", "c:\\halo\\SOURCE\\networking\\network_connection.c", 0x1f7, true);
    system_exit(-1);
  }
  if (client == NULL) {
    display_assert("client_connection", "c:\\halo\\SOURCE\\networking\\network_connection.c", 0x1f8, true);
    system_exit(-1);
  }
  if ((server->connection.flags & FLAG(_connection_create_server_bit)) == 0) {
    display_assert("server_connection->flags & FLAG(_connection_create_server_bit)",
                   "c:\\halo\\SOURCE\\networking\\network_connection.c", 0x1f9, true);
    system_exit(-1);
  }
  if (server->endpoint_set == 0) {
    display_assert("server->endpoint_set", "c:\\halo\\SOURCE\\networking\\network_connection.c", 0x1fa, true);
    system_exit(-1);
  }
  slot = server->client_list;
  if (slot == NULL) {
    display_assert("server->client_list", "c:\\halo\\SOURCE\\networking\\network_connection.c", 0x1fb, true);
    system_exit(-1);
  }

  i = 0;
  while (*slot == NULL || *slot != client) {
    i++;
    slot++;
    if (i >= 5) {
      return false;
    }
  }

  if (client->reliable_endpoint != 0) {
    result = remove_endpoint_from_set(
      (int *)(*slot)->reliable_endpoint,
      (uint32_t *)server->endpoint_set);
    if (result != 0) {
      error(2,
            "failed to remove a client endpoint from the server's endpoint set "
            "(maybe it was already removed)");
    }
  }

  network_connection_delete((int)*slot);
  *slot = NULL;
  return true;
}

/* network_connection_read (0x1298f0)
 *
 * Read a message from a network connection. If the connection is a server,
 * reads from the unreliable incoming queue. If the connection is a client,
 * tries the reliable queue first, then falls back to unreliable if the
 * connection is a clientside client.
 *
 * NOTE: __FILE__ string references network_connection.c, but this function
 * is linked into network_game_globals.obj.
 */
bool network_connection_read(int connection, void *buffer, int *size, void *addr)
{
  network_connection *conn;
  bool result;

  conn = (network_connection *)connection;

  if ((conn->flags & FLAG(_connection_create_server_bit)) != 0) {
    return network_client_unreliable_connection_read(connection, buffer, size, addr);
  }

  if ((conn->flags & (FLAG(_connection_create_clientside_client_bit) |
                      FLAG(_connection_create_serverside_client_bit))) == 0) {
    assert_halt_msg(
      0, "connection->flags&FLAG(_connection_create_clientside_client_bit) || "
         "connection->flags&FLAG(_connection_create_serverside_client_bit)");
  }

  result = network_client_reliable_connection_read(connection, buffer, size, addr);
  if (!result && (conn->flags & FLAG(_connection_create_clientside_client_bit)) != 0) {
    result = network_client_unreliable_connection_read(connection, buffer, size, addr);
  }
  return result;
}

/* network_connection_disconnect (0x129980)
 *
 * Resets a network connection's endpoints. Called from
 * network_game_client_reset (0x1267c0) and network_game_client_leave_game
 * (0x126140).
 *
 * If the connection is currently connected, runs the
 * network_connection_idle_client_reliable_endpoint teardown when the
 * connection's +0x30 flag byte has bit 1 or 2 set, then closes the active
 * endpoint stored at +0x00.
 *
 * If the connection has a secondary endpoint (+0x04) and a non-zero bound port
 * (+0x34), it destroys that endpoint and recreates one from endpoint set 0x11,
 * rebinding it to an address record { address = 0, type = 4, port }. Returns
 * true on success (or when there is nothing to rebind), false if the endpoint
 * could not be recreated or rebound.
 */
bool network_connection_disconnect(int connection)
{
  network_connection *conn;
  int addr[6];
  int new_endpoint;
  short port;

  conn = (network_connection *)connection;

  if (network_connection_connected(connection)) {
    if ((conn->flags & (FLAG(_connection_create_clientside_client_bit) |
                        FLAG(_connection_create_serverside_client_bit))) != 0) {
      network_connection_idle_client_reliable_endpoint(connection);
    }
    close_endpoint((int *)conn->reliable_endpoint);
  }

  if (conn->unreliable_endpoint != 0) {
    port = conn->well_known_port;
    if (port != 0) {
      *(short *)((char *)addr + 0x10) = 4;
      addr[0] = 0;
      *(short *)((char *)addr + 0x12) = port;
      destroy_endpoint((int *)conn->unreliable_endpoint);
      new_endpoint = get_next_endpoint_from_set(0x11);
      conn->unreliable_endpoint = new_endpoint;
      if (new_endpoint != 0) {
        if (FUN_00083ce0((int *)new_endpoint, addr) == 0) {
          if (FUN_00083bd0(conn->unreliable_endpoint, 0) == 0) {
            return true;
          }
        }
      }
      return false;
    }
  }
  return true;
}

/* network_connection_idle (0x129cf0)
 *
 * Network connection idle processing. Handles timeout detection, reliable and
 * unreliable endpoint servicing, and datagram reception from the network
 * endpoint into the circular queue.
 *
 * NOTE: __FILE__ string references network_connection.c, but this function
 * is linked into network_game_globals.obj.
 */
bool network_connection_idle(int connection, int timeout, int *output)
{
  network_connection *conn;
  unsigned int now;
  uint32_t raw_flags;
  uint32_t flags;
  bool ok;
  bool is_connected;
  int bytes_read;
  short addr_result;
  int elapsed;
  double dval;
  unsigned int queue_space;
  uint8_t recv_buf[400];
  uint8_t addr_buf[24];

  now = system_milliseconds();
  ok = true;

  assert_halt(connection);
  conn = (network_connection *)connection;

  raw_flags = conn->flags;
  flags = raw_flags & ~0x20u;
  conn->flags = flags;

  if (timeout != 0) {
    if (now > (unsigned int)(conn->last_keep_alive_milliseconds + 5000)) {
      conn->flags = flags | 0x20;
    }
    if (now > (unsigned int)(conn->last_keep_alive_milliseconds + timeout)) {
      if (!network_connection_dont_timeout) {
        error(2, "timeout in network_connection_idle");
        return false;
      }
      error(2, "dont timeout is active so not timing out of a connection");
      conn->last_keep_alive_milliseconds = now;
    }
  } else {
    conn->last_keep_alive_milliseconds = now;
  }

  if ((conn->flags & 1) != 0) {
    ok = network_connection_idle_server_reliable_endpoint(connection, output);
    if (!ok) {
      error(2, "network_connection_idle_server_reliable_endpoint failed");
      return false;
    }
  } else if ((conn->flags & 6) != 0) {
    ok = network_connection_idle_client_reliable_endpoint(connection);
    if (!ok) {
      error(2, "network_connection_idle_client_reliable_endpoint failed");
      return false;
    }
  }

  if (conn->unreliable_endpoint == 0)
    return ok;

  queue_space = circular_queue_free_space(conn->unreliable_incoming_queue);

  while (ok && queue_space >= 0x194) {
    bytes_read = 0;
    is_connected = FUN_000831a0(conn->unreliable_endpoint);

    if (is_connected) {
      bytes_read =
        recv_endpoint((int *)conn->unreliable_endpoint, recv_buf, 400);
      if (bytes_read > 0) {
        addr_result = FUN_00083a60((int *)conn->unreliable_endpoint, addr_buf);
        if (addr_result != 0) {
          csmemset(addr_buf, 0, 0x18);
          ((transport_address *)addr_buf)->address_length = 4;
        }

        if (conn->traffic_log_file != NULL) {
          elapsed =
            (int)(system_milliseconds() - conn->traffic_log_start_milliseconds);
          dval = (double)(unsigned int)elapsed * 0.001;
          crt_fprintf(conn->traffic_log_file, "%g\t%ld\t%ld\t%ld\t%ld\n",
                      dval, 0, bytes_read, 0, 0);
          crt_fflush(conn->traffic_log_file);
        }
        conn->datagrams_received = conn->datagrams_received + 1;
      }
    } else {
      bytes_read = FUN_00084520((transport_endpoint *)conn->unreliable_endpoint,
                                recv_buf, 400, (transport_address *)addr_buf);
      if (bytes_read > 0) {
        if (conn->traffic_log_file != NULL) {
          elapsed =
            (int)(system_milliseconds() - conn->traffic_log_start_milliseconds);
          dval = (double)(unsigned int)elapsed * 0.001;
          crt_fprintf(conn->traffic_log_file, "%g\t%ld\t%ld\t%ld\t%ld\n",
                      dval, 0, bytes_read, 0, 0);
          crt_fflush(conn->traffic_log_file);
        }
        conn->datagrams_received = conn->datagrams_received + 1;
      }
    }

    if (bytes_read > 400) {
      assert_halt_msg(0, "endpoint read buffer overflowed");
    }

    if (bytes_read <= 0)
      return ok;

    if (*(int *)addr_buf == 0) {
      error(2, "datagram received from unknown address");
    } else {
      csmemcpy(recv_buf + bytes_read, addr_buf, 4);
      ok = FUN_00118ec0(conn->unreliable_incoming_queue, recv_buf, bytes_read + 4);
      if (!ok) {
        assert_halt_msg(
          0, "circular_queue_queue_data() failed though it should have had "
             "enough room");
      }
    }

    queue_space = circular_queue_free_space(conn->unreliable_incoming_queue);
  }

  return ok;
}

/* network_game_in_progress (0x12a000)
 *
 * Returns true when either global network game endpoint pointer is non-null.
 */
int network_game_in_progress(void)
{
  if (network_game_globals.client == NULL && network_game_globals.server == NULL) {
    return 0;
  }

  return 1;
}

/* Set the number of games played in both server and client game globals.
 * 0x12a020 / network_game_globals.obj
 */
void network_game_set_number_of_games_played(int games_played)
{
  int game;
  int server = (int)network_game_globals.server;
  if (server != 0) {
    game = network_game_server_get_game((void *)server);
    *(int *)(game + 0x42c) = games_played;
  }
  server = (int)network_game_globals.client;
  if (server != 0) {
    game = (int)network_game_client_get_game((void *)server);
    *(int *)(game + 0x42c) = games_played;
  }
}

/* Set the random seed in both server and client game globals.
 * 0x12a060 / network_game_globals.obj
 */
void network_game_set_random_seed(int seed)
{
  int game;
  int server = (int)network_game_globals.server;
#ifdef HALO_RNG_TRACE
  RNG_TRACE(RNG_TRACE_GLOBAL_SEED_ADDR, RNG_TRACE_KIND_NET_SET_SEED, seed);
#endif
#line 306
  if (server != 0) {
    game = network_game_server_get_game((void *)server);
    *(int *)(game + 0x428) = seed;
  }
  server = (int)network_game_globals.client;
  if (server != 0) {
    game = (int)network_game_client_get_game((void *)server);
    *(int *)(game + 0x428) = seed;
  }
}
#line 316

/* Return the active game object: server's game if server exists,
 * else client's game, else NULL.
 * 0x12a0a0 / network_game_globals.obj */
int network_game_get_game(void)
{
  int result;

  if (network_game_globals.server != NULL) {
    result = network_game_server_get_game(network_game_globals.server);
    return result;
  }
  if (network_game_globals.client != NULL) {
    result = (int)network_game_client_get_game(network_game_globals.client);
    return result;
  }
  return 0;
}

/* network_game_player_is_local (0x12a0d0)
 *
 * Returns whether the given player is local to this machine.
 *
 * If the player is valid and a network game client exists, the player is
 * local when the client's active machine index (machine byte at +0x40)
 * matches the player's machine index (player byte at +0x1c).
 *
 * Otherwise, when not connected as a client (game_connection() != 3, i.e.
 * single-player or host), every player is treated as local. When connected
 * as a client, machine index 0 (player byte +0x1c == 0) is the local machine.
 * A NULL player in the client case triggers an assert/halt.
 *
 * Source: network_game_globals.c line 0x9b (155).
 */
bool network_game_player_is_local(void *player)
{
  network_player_record_t *player_record =
    (network_player_record_t *)player;
  void *machine;

  if (player != NULL && network_player_is_valid(player) &&
      network_game_globals.client != NULL) {
    machine = network_game_client_get_machine(network_game_globals.client);
    if (machine != NULL &&
        *(char *)((char *)machine + 0x40) == player_record->machine_index) {
      return true;
    }
    return false;
  }

  if (game_connection() == 3) {
    assert_halt_at("c:\\halo\\SOURCE\\networking\\network_game_globals.c", 0x9b,
                   player);

    return player_record->machine_index == '\0';
  }

  return true;
}

/* network_game_set_accept_remote_connections (0x12a150)
 *
 * Stores the one-byte "accept remote connections" flag to the network game
 * globals byte at 0x46e8c4 (the byte read back by
 * network_game_should_accept_remote_connections at 0x12a160).
 */
void network_game_set_accept_remote_connections(char accept)
{
  network_game_globals.accept_remote_connections = accept;
}

/* network_game_should_accept_remote_connections (0x12a160)
 *
 * Returns the network game globals byte at 0x46e8c4.
 */
bool network_game_should_accept_remote_connections(void)
{
  return network_game_globals.accept_remote_connections;
}

/* network_game_is_splitscreen_local (0x12a170)
 *
 * Returns true when a server connection exists (0x46e8bc != NULL) and the
 * global byte at 0x46e8c4 is zero.
 */
int network_game_is_splitscreen_local(void)
{
  if (network_game_globals.server != NULL && !network_game_globals.accept_remote_connections) {
    return 1;
  }
  return 0;
}

/* Set the quickstart-local flag (0x46e8c5) to 1.
 * 0x12a190 / network_game_globals.obj */
void network_game_set_quickstart_local(void)
{
  network_game_globals.quickstart_local = 1;
}

/* Return true if this is a quickstart-local session:
 * server exists AND not accepting remote connections AND quickstart flag set.
 * 0x12a1a0 / network_game_globals.obj */
unsigned int network_game_is_quickstart_local(void)
{
  if ((network_game_globals.server == NULL) ||
      (network_game_globals.accept_remote_connections != 0) ||
      (network_game_globals.quickstart_local != 1)) {
    return 0;
  }
  return 1;
}

/* global_network_game_server_get (0x12a1d0)
 *
 * Returns the global network game server pointer.
 */
void *global_network_game_server_get(void)
{
  return network_game_globals.server;
}

/* dispose_global_network_game_server (0x12a1e0)
 *
 * Tear down the global network game client if one exists.
 */
void dispose_global_network_game_server(void)
{
  void *client = network_game_globals.server;

  if (client != NULL) {
    network_game_server_dispose(client);
    network_game_globals.server = NULL;
    network_game_globals.quickstart_local = 0;
  }
}

/* network_game_server_start_frame (0x12a210)
 *
 * Begin a network game server frame.  Returns the result of the
 * server's idle function, or true with a warning if no server exists.
 */
bool network_game_server_start_frame(void)
{
  if (network_game_globals.server != NULL) {
    return network_game_server_idle(network_game_globals.server);
  }
  error(2, "no network game server");
  return true;
}

/* global_network_game_client_get (0x12a240)
 *
 * Returns the global network game client pointer.
 */
void *global_network_game_client_get(void)
{
  return network_game_globals.client;
}

/* Create and initialize the global network game client.
 * Asserts the client slot is empty, then allocates via network_game_client_create.
 * 0x12a250 / network_game_globals.obj */
bool create_global_network_game_client(void)
{
  if (network_game_globals.client != NULL) {
    display_assert("global_network_game_client==NULL",
                   "c:\\halo\\SOURCE\\networking\\network_game_globals.c",
                   0x10f, 1);
    system_exit(-1);
  }
  network_game_globals.client = network_game_client_create();
  if (network_game_globals.client != NULL) {
    network_game_globals.abort = 0;
  }
  return network_game_globals.client != NULL;
}

/* dispose_global_network_game_client (0x12a2a0)
 *
 * Tear down the global network game server if one exists.
 */
void dispose_global_network_game_client(void)
{
  if (network_game_globals.client != NULL) {
    network_game_client_dispose(network_game_globals.client);
    network_game_globals.client = NULL;
  }
  network_game_globals.abort = 0;
}

/* network_game_client_start_frame (0x12a2d0)
 *
 * Begin a network game client frame.  If the abort flag (0x46e8c6) is
 * set, performs an orderly disconnect: disposes both server and client,
 * then returns true.  Otherwise, polls the server for its current state
 * and logs state transitions.
 */
bool network_game_client_start_frame(void)
{
  int16_t state;
  int16_t out; /* network_game_client_get_state writes a word at 0x124a63/0x124a91 */
  int reason;
  bool result;

  if (network_game_globals.abort == 1) {
    set_game_connection(0);

    if (network_game_globals.server != NULL) {
      reason = network_game_server_get_game(network_game_globals.server);
    } else if (network_game_globals.client != NULL) {
      reason = (int)network_game_client_get_game(network_game_globals.client);
    } else {
      reason = 0;
    }
    network_game_end_and_load_ui((void *)reason);

    if (network_game_globals.client != NULL) {
      network_game_client_dispose(network_game_globals.client);
      network_game_globals.client = NULL;
    }
    network_game_globals.abort = 0;
    if (network_game_globals.server != NULL) {
      network_game_server_dispose(network_game_globals.server);
      network_game_globals.server = NULL;
      network_game_globals.quickstart_local = 0;
    }
    main_goto_main_menu();
    return true;
  }

  result = network_game_client_idle(network_game_globals.client);
  if (!result) {
    network_event(
      "internal networking error [network_game_client_idle() failed]");
    return false;
  }

  if (network_game_client_get_error(network_game_globals.client) != 0) {
    network_event(
      "internal networking error [network_game_client_get_error()!=0]");
    return false;
  }

  state = network_game_client_get_state(network_game_globals.client, &out);

  switch (state) {
  case _network_game_client_state_searching:
    if (s_last_client_state != state)
      network_event("searching for a network game ...");
    break;
  case _network_game_client_state_joining:
    if (s_last_client_state != state)
      network_event("joining a network game ...");
    break;
  case _network_game_client_state_pregame:
    if (s_last_client_state != state)
      network_event("waiting for game to start ...");
    break;
  case _network_game_client_state_ingame:
    if (s_last_client_state != state)
      network_event("client signalled to begin loading for network game");
    break;
  case _network_game_client_state_postgame:
    if (s_last_client_state != state)
      network_event("waiting for game to restart ...");
    break;
  default:
    display_assert("client is in an unknown state",
                   "c:\\halo\\SOURCE\\networking\\network_game_globals.c",
                   0x160, 1);
    system_exit(-1);
    break;
  }
  s_last_client_state = state;
  return result;
}

/* network_game_client_end_frame (0x12a500)
 *
 * End a network game client frame.  If no server exists, resets player
 * actions.  Otherwise, in state 3 (in-game), sends a game update packet
 * to the server at most every 16ms.
 */
bool network_game_client_end_frame(void)
{
  int16_t state;
  int now;
  bool result;
  uint32_t flags;
  uint16_t *msg;
  uint8_t out_buf[128]; /* local_124, EBP-0x124 */
  uint32_t msg_buf[34];
  uint8_t addr_buf[24]; /* local_1c, EBP-0x1c */

  result = true;

  if (network_game_globals.client == NULL) {
    set_game_connection(0);
    main_reset_player_actions();
    return true;
  }

  state = network_game_client_get_state(network_game_globals.client, NULL);

  if (state == 3) {
    now = system_milliseconds();

    if ((unsigned int)(now - network_game_globals.last_update_time) >= 0x10 &&
        network_game_client_server_has_started_game(network_game_globals.client)) {
      network_game_client_get_next_update_number(network_game_globals.client);
      network_game_client_get_game(network_game_globals.client);
      update_client_build_client_update(out_buf);

      if (network_client_get_oos(network_game_globals.client)) {
        flags = network_game_client_get_next_update_number(network_game_globals.client);
        flags = flags | 0x80000000;
      } else {
        flags = network_game_client_get_next_update_number(network_game_globals.client);
        flags = flags & 0x7fffffff;
      }

      msg_buf[0] = flags;
      csmemcpy((char *)msg_buf + 8, out_buf, 0x80);
      *(uint16_t *)((char *)msg_buf + 6) = (uint16_t)local_player_count();
#ifdef HALO_RNG_TRACE
      RNG_TRACE_EX(RNG_TRACE_KIND_NET_UPDATE_FLAGS, flags,
                   (unsigned int)*(uint16_t *)((char *)msg_buf + 6));
      RNG_TRACE_EX(RNG_TRACE_KIND_NET_UPDATE_BUTTONS_01,
                   *(uint32_t *)(out_buf + 0x00),
                   *(uint32_t *)(out_buf + 0x20));
      RNG_TRACE_EX(RNG_TRACE_KIND_NET_UPDATE_BUTTONS_23,
                   *(uint32_t *)(out_buf + 0x40),
                   *(uint32_t *)(out_buf + 0x60));
#endif

      msg = (uint16_t *)create_network_game_message(0x19, msg_buf, 0x88);

      if (msg != NULL) {
        network_game_client_get_remote_server_address(network_game_globals.client, addr_buf);
        /* arg1 is the client's connection handle at +0x82c, fetched via the
         * 0x125710 getter (its kb name is a misnomer; it returns
         * *(client+0x82c), the same send channel FUN_001263a0 passes to
         * FUN_00128e00). arg4 is the address of the local_1c record filled by
         * switch_to_postgame. */
        result =
          network_game_client_write((void *)network_game_client_get_connection(
                         network_game_globals.client),
                       msg, *msg >> 4, (int)addr_buf, 0);
        if (!result) {
          network_event("failed to send a game update to the server");
          network_game_globals.last_update_time = now;
          return false;
        }
      } else {
        network_event(
          "failed to create a _message_type_client_game_update message");
        result = false;
      }
      network_game_globals.last_update_time = now;
    }
  }

  return result;
}

/* network_game_client_get_local_machine_index (0x12a690)
 *
 * Returns the local machine index for the network game client, or -1 if the
 * client is absent or has no valid machine record. The machine record is
 * fetched via network_game_client_get_machine(); the index is a signed byte
 * at machine+0x40, sign-extended to short.
 * Source: network_game_globals.c
 */
short network_game_client_get_local_machine_index(void)
{
  void *machine;
  short result;

  result = -1;
  if (network_game_globals.client != NULL) {
    machine = network_game_client_get_machine(network_game_globals.client);
    if (machine != NULL) {
      result = (short)*(char *)((char *)machine + 0x40);
    }
  }
  return result;
}

/* network_game_client_local_player_quit (0x12a6c0)
 *
 * Handles a local player quitting an in-progress network game. Walks the
 * client's 16-slot player table (records at index_base + 0x226, stride 0x20)
 * looking for the valid player record whose machine index (+0x40 on the
 * machine object) matches this client's machine and whose controller index
 * (record byte +1) matches the requested player. On a match, requests that
 * player's removal via the network client; logs an error if the request fails.
 */
void network_game_client_local_player_quit(short player)
{
  void *machine;
  void *index_base;
  char *slot;
  char *record;
  int i;

  if (network_game_globals.client != NULL) {
    machine = network_game_client_get_machine(network_game_globals.client);
    index_base = network_game_client_get_game(network_game_globals.client);
    if (machine != NULL) {
      i = 0;
      slot = (char *)index_base + 0x242;
      while (!network_player_is_valid(slot - 0x1c) ||
             *slot != *((char *)machine + 0x40) || slot[1] != player) {
        i = i + 1;
        slot = slot + 0x20;
        if (0xf < i) {
          return;
        }
      }
      record = (char *)index_base + i * 0x20 + 0x226;
      if (record != NULL && !network_game_client_request_remove_player(
                              network_game_globals.client, record)) {
        error(2, "failed to request player removal in-game for player #%d",
              (int)*(char *)(record + 0x1d));
      }
    }
  }
}

/* network_game_abort (0x12a780)
 *
 * Signals network-game abort by setting the global abort flag byte.
 */
void network_game_abort(void)
{
  network_game_globals.abort = 1;
}

/* network_game_client_all_local_players_have_quit (0x12a790)
 *
 * Sets the network-game abort flag byte when all local players have quit.
 */
void network_game_client_all_local_players_have_quit(void)
{
  network_game_globals.abort = 1;
}

/* Request a game start from the network client (request_type=3).
 * Logs a warning if the request fails.
 * 0x12a7a0 / network_game_globals.obj */
void network_game_client_request_immediate_start(void)
{
  if (network_game_globals.client != NULL) {
    if (!network_game_client_request_start_time_change(network_game_globals.client,
                                                       3)) {
      error(2, "network_game_client_request_start() failed");
    }
  }
}

/* Return the number of games played from the active network game globals.
 * Resolves the server's game globals if a server exists, otherwise the
 * client's; asserts a non-null game pointer was resolved, then reads the
 * field at game+0x428.
 * 0x12a830 / network_game_globals.obj */
int network_game_get_random_seed(void)
{
  int game;

  if (network_game_globals.server != NULL) {
    game = network_game_server_get_game(network_game_globals.server);
  } else if (network_game_globals.client != NULL) {
    game = (int)network_game_client_get_game(network_game_globals.client);
  } else {
    game = 0;
  }

  if (game == 0) {
    display_assert(
      "game", "c:\\halo\\SOURCE\\networking\\network_game_globals.c", 0x73, 1);
    system_exit(-1);
  }

  return *(int *)(game + 0x428);
}

/* Create and initialize the global network game server.
 * Asserts the server slot is empty, allocates via network_game_server_create,
 * then seeds both server and client with a random step value.
 * 0x12a890 / network_game_globals.obj */
bool create_global_network_game_server(void)
{
  unsigned int *seed_addr;
  unsigned int seed_step;
  int game;

  if (network_game_globals.server != NULL) {
    display_assert("global_network_game_server==NULL",
                   "c:\\halo\\SOURCE\\networking\\network_game_globals.c", 0xd6,
                   1);
    system_exit(-1);
  }
  network_game_globals.server = network_game_server_create();
  if (network_game_globals.server != NULL) {
    seed_addr = random_math_get_local_seed_address();
    seed_step = (unsigned int)random_seed_step(seed_addr);
    if (network_game_globals.server != NULL) {
      game = network_game_server_get_game(network_game_globals.server);
      *(unsigned int *)(game + 0x428) = seed_step;
    }
    if (network_game_globals.client != NULL) {
      game = (int)network_game_client_get_game(network_game_globals.client);
      *(unsigned int *)(game + 0x428) = seed_step;
    }
  }
  return network_game_globals.server != NULL;
}

/* network_game_invalidate_player (0x12a920)
 *
 * Resets a network player entry: clears the player index (uint16 at offset 0)
 * to 0 and marks bytes at offsets 0x1c-0x1f as 0xFF (invalid/unused sentinel).
 * Source: network_game_manager.c line 88.
 */
void network_game_invalidate_player(uint8_t *player)
{
  if (player == NULL) {
    display_assert("player",
                   "c:\\halo\\SOURCE\\networking\\network_game_manager.c", 0x58,
                   1);
    system_exit(-1);
  }
  player[0x1c] = 0xff;
  player[0x1d] = 0xff;
  player[0x1e] = 0xff;
  player[0x1f] = 0xff;
  *(uint16_t *)player = 0;
}

/* 0x12a7d0 — network_game_get_number_of_games_played */
int network_game_get_number_of_games_played(void)
{
  void *server;
  void *client;
  char *game;

  server = *(void **)0x0046e8bc;
  if (server != NULL) {
    game = (char *)(uintptr_t)network_game_server_get_game(server);
  } else {
    client = *(void **)0x0046e8c0;
    if (client != NULL) {
      game = (char *)network_game_client_get_game(client);
    } else {
      game = NULL;
    }
  }

  if (game != NULL) {
    return *(int *)(game + 0x42c);
  }

  display_assert("game",
                 "c:\\halo\\SOURCE\\networking\\network_game_globals.c",
                 0x55, 1);
  system_exit(-1);
  return *(int *)(game + 0x42c);
}

