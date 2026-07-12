// ssh-compat.c
// SSH compatibility and I/O context management
//
// Copyright (C) 2026 Roan Rothrock
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <libssh/libssh.h>
#include <libssh/server.h>

#include <liblimeade.h>

enum LIMEADE_ROLE
{
  LIMEADE_ROLE_HOST = 1,
  LIMEADE_ROLE_CLIENT = 2
};

typedef struct
{
  uint8_t type;
  uint16_t datasz_before;
  uint16_t datasz_after;
} LIMEADE_WIRE_FLAGS;

struct LIMEADE_CONTEXT_IMPL
{
  enum LIMEADE_ROLE role;
  ssh_bind bind;
  ssh_session session;
  ssh_channel channel;
  uint8_t session_id[5];
  bool has_session_id;
  int timeout_ms;
  unsigned int port;
};

#define LIMEADE_MAGIC_WIRE_LEN 7
#define LIMEADE_FLAGS_WIRE_LEN 7
#define LIMEADE_SESSION_WIRE_LEN 5
#define LIMEADE_MAX_DATASZ 16384 // 2 ** 14
#define LIMEADE_SUBSYSTEM_NAME "limeade"

static LIMEADE_PACKET limeade_empty_packet(void)
{
  LIMEADE_PACKET p;

  p.sz = 0;
  p.data = NULL;
  return p;
}

static void limeade_generate_session_id(uint8_t out[LIMEADE_SESSION_WIRE_LEN])
{
  /* Generates pseudo-random session ID */
  size_t i;

  srand((unsigned int)(time(NULL) ^ (unsigned int)getpid()));
  for (i = 0; i < LIMEADE_SESSION_WIRE_LEN; i++)
  {
    out[i] = (uint8_t)(rand() & 0xFF);
  }
}

static int limeade_parse_wire_flags(const uint8_t raw[LIMEADE_FLAGS_WIRE_LEN],
                                    LIMEADE_WIRE_FLAGS *out)
{
  /* Converts raw packet data into a LIMEADE_WIRE_FLAGS object */
  uint8_t b1;
  uint8_t b2;
  uint8_t b3;
  uint8_t b4;
  uint16_t before;
  uint16_t after;

  if (raw == NULL || out == NULL)
  {
    return -1;
  }

  b1 = raw[1];
  b2 = raw[2];
  b3 = raw[3];
  b4 = raw[4];

  out->type = (uint8_t)((b1 >> 4) & 0x0F);

  before = (uint16_t)(b1 & 0x0F);
  before = (uint16_t)((before << 8) | b2);
  before = (uint16_t)((before << 2) | ((b3 >> 6) & 0x03));

  after = (uint16_t)(b3 & 0x3F);
  after = (uint16_t)((after << 8) | b4);

  out->datasz_before = before;
  out->datasz_after = after;

  return 0;
}

static int limeade_read_exact(LIMEADE_CONTEXT ctx, void *buf, size_t need)
{
  size_t off;

  if (ctx == NULL || buf == NULL)
  {
    limeade_inserror(LIMEADE_ERROR_INVALID_CONTEXT);
    return -1;
  }

  off = 0;
  while (off < need)
  {
    int rc;

    if (ctx->timeout_ms >= 0)
    {
      rc = ssh_channel_read_timeout(ctx->channel, (char *)buf + off,
                                    (uint32_t)(need - off), 0, ctx->timeout_ms);
    }
    else
    {
      rc = ssh_channel_read(ctx->channel, (char *)buf + off,
                            (uint32_t)(need - off), 0);
    }

    if (rc == SSH_ERROR)
    {
      limeade_inserror(LIMEADE_ERROR_SSH_IO);
      return -1;
    }

    if (rc == SSH_AGAIN)
    {
      limeade_inserror(LIMEADE_ERROR_TIMEOUT);
      return -1;
    }

    if (rc == 0)
    {
      limeade_inserror(LIMEADE_ERROR_SSH_IO);
      return -1;
    }

    off += (size_t)rc;
  }

  return 0;
}

static int limeade_write_exact(LIMEADE_CONTEXT ctx, const void *buf,
                               size_t need)
{
  size_t off;

  if (ctx == NULL || buf == NULL)
  {
    limeade_inserror(LIMEADE_ERROR_INVALID_CONTEXT);
    return -1;
  }

  off = 0;
  while (off < need)
  {
    int rc;

    rc = ssh_channel_write(ctx->channel, (const char *)buf + off,
                           (uint32_t)(need - off));
    if (rc == SSH_ERROR || rc <= 0)
    {
      limeade_inserror(LIMEADE_ERROR_SSH_IO);
      return -1;
    }

    off += (size_t)rc;
  }

  return 0;
}

static bool limeade_should_validate_session(uint8_t packet_type)
{
  if (packet_type == (uint8_t)LIMEADE_CLIENT_ASK)
  {
    return false;
  }

  if (packet_type == (uint8_t)LIMEADE_HOST_ANSWER)
  {
    return false;
  }

  return true;
}

static int limeade_send_common(LIMEADE_CONTEXT ctx, LIMEADE_PACKET pckt,
                               enum LIMEADE_ROLE role)
{
  LIMEADE_WIRE_FLAGS flags;
  const uint8_t *wire;
  size_t min_header;
  size_t session_len;

  if (ctx == NULL || ctx->role != role || ctx->channel == NULL)
  {
    limeade_inserror(LIMEADE_ERROR_INVALID_CONTEXT);
    return -1;
  }

  if (pckt.data == NULL || pckt.sz == 0)
  {
    limeade_inserror(LIMEADE_ERROR_PACKET_FORMAT);
    return -1;
  }

  wire = (const uint8_t *)pckt.data;
  min_header = LIMEADE_MAGIC_WIRE_LEN + LIMEADE_FLAGS_WIRE_LEN;
  if ((size_t)pckt.sz < min_header)
  {
    limeade_inserror(LIMEADE_ERROR_PACKET_FORMAT);
    return -1;
  }

  if (memcmp(wire, LIMEADE_MAGIC, LIMEADE_MAGIC_WIRE_LEN) != 0)
  {
    limeade_inserror(LIMEADE_ERROR_PACKET_MAGIC);
    return -1;
  }

  if (limeade_parse_wire_flags(wire + LIMEADE_MAGIC_WIRE_LEN, &flags) < 0)
  {
    limeade_inserror(LIMEADE_ERROR_PACKET_FORMAT);
    return -1;
  }

  session_len = (flags.type == (uint8_t)LIMEADE_CLIENT_ASK)
                    ? 0
                    : LIMEADE_SESSION_WIRE_LEN;
  if ((size_t)pckt.sz < min_header + session_len)
  {
    limeade_inserror(LIMEADE_ERROR_PACKET_FORMAT);
    return -1;
  }

  if (session_len > 0)
  {
    if (!ctx->has_session_id)
    {
      limeade_inserror(LIMEADE_ERROR_PACKET_SESSION);
      return -1;
    }

    memcpy((uint8_t *)pckt.data + min_header, ctx->session_id,
           LIMEADE_SESSION_WIRE_LEN);
  }

  if (limeade_write_exact(ctx, pckt.data, (size_t)pckt.sz) < 0)
  {
    return -1;
  }

  limeade_inserror(LIMEADE_SUCCESS);
  return (int)pckt.sz;
}

static LIMEADE_PACKET limeade_recv_common(LIMEADE_CONTEXT ctx,
                                          enum LIMEADE_ROLE role)
{
  LIMEADE_PACKET ret;
  LIMEADE_WIRE_FLAGS flags;
  uint8_t magic[LIMEADE_MAGIC_WIRE_LEN];
  uint8_t flags_raw[LIMEADE_FLAGS_WIRE_LEN];
  uint8_t session_id[LIMEADE_SESSION_WIRE_LEN];
  size_t payload_len;
  size_t session_len;
  size_t total_len;
  uint8_t *dst;

  ret = limeade_empty_packet();

  if (ctx == NULL || ctx->role != role || ctx->channel == NULL)
  {
    limeade_inserror(LIMEADE_ERROR_INVALID_CONTEXT);
    return ret;
  }

  if (limeade_read_exact(ctx, magic, LIMEADE_MAGIC_WIRE_LEN) < 0)
  {
    return ret;
  }

  if (memcmp(magic, LIMEADE_MAGIC, LIMEADE_MAGIC_WIRE_LEN) != 0)
  {
    limeade_inserror(LIMEADE_ERROR_PACKET_MAGIC);
    return ret;
  }

  if (limeade_read_exact(ctx, flags_raw, LIMEADE_FLAGS_WIRE_LEN) < 0)
  {
    return ret;
  }

  if (limeade_parse_wire_flags(flags_raw, &flags) < 0)
  {
    limeade_inserror(LIMEADE_ERROR_PACKET_FORMAT);
    return ret;
  }

  if (flags.datasz_before > LIMEADE_MAX_DATASZ ||
      flags.datasz_after > LIMEADE_MAX_DATASZ)
  {
    limeade_inserror(LIMEADE_ERROR_PACKET_SIZE);
    return ret;
  }

  session_len = (flags.type == (uint8_t)LIMEADE_CLIENT_ASK)
                    ? 0
                    : LIMEADE_SESSION_WIRE_LEN;

  if (session_len > 0)
  {
    if (limeade_read_exact(ctx, session_id, session_len) < 0)
    {
      return ret;
    }
  }

  if (limeade_should_validate_session(flags.type) && ctx->has_session_id &&
      session_len > 0)
  {
    if (memcmp(ctx->session_id, session_id, LIMEADE_SESSION_WIRE_LEN) != 0)
    {
      limeade_inserror(LIMEADE_ERROR_PACKET_SESSION);
      return ret;
    }
  }

  if (flags.type == (uint8_t)LIMEADE_HOST_ANSWER && session_len > 0)
  {
    memcpy(ctx->session_id, session_id, LIMEADE_SESSION_WIRE_LEN);
    ctx->has_session_id = true;
  }

  payload_len =
      (flags.datasz_after > 0) ? flags.datasz_after : flags.datasz_before;

  if (payload_len > LIMEADE_MAX_DATASZ)
  {
    limeade_inserror(LIMEADE_ERROR_PACKET_SIZE);
    return ret;
  }

  total_len = LIMEADE_MAGIC_WIRE_LEN + LIMEADE_FLAGS_WIRE_LEN + session_len +
              payload_len;
  if (total_len > UINT32_MAX)
  {
    limeade_inserror(LIMEADE_ERROR_PACKET_SIZE);
    return ret;
  }

  ret.data = (unsigned char *)malloc(total_len);
  if (ret.data == NULL)
  {
  	return limeade_empty_packet();
  }

  dst = ret.data;
  memcpy(dst, magic, LIMEADE_MAGIC_WIRE_LEN);
  dst += LIMEADE_MAGIC_WIRE_LEN;
  memcpy(dst, flags_raw, LIMEADE_FLAGS_WIRE_LEN);
  dst += LIMEADE_FLAGS_WIRE_LEN;

  if (session_len > 0)
  {
    memcpy(dst, session_id, session_len);
    dst += session_len;
  }

  if (payload_len > 0)
  {
    if (limeade_read_exact(ctx, dst, payload_len) < 0)
    {
      free(ret.data);
      return limeade_empty_packet();
    }
  }

  ret.sz = (uint32_t)total_len;
  limeade_inserror(LIMEADE_SUCCESS);
  return ret;
}

static void limeade_context_free_common(LIMEADE_CONTEXT ctx)
{
  if (ctx == NULL)
  {
    return;
  }

  if (ctx->channel != NULL)
  {
    ssh_channel_send_eof(ctx->channel);
    ssh_channel_close(ctx->channel);
    ssh_channel_free(ctx->channel);
  }

  if (ctx->session != NULL)
  {
    ssh_disconnect(ctx->session);
    ssh_free(ctx->session);
  }

  if (ctx->bind != NULL)
  {
    ssh_bind_free(ctx->bind);
  }

  free(ctx);
}

static void limeade_context_cleanup_on_error(LIMEADE_CONTEXT ctx, ssh_key key)
{
  if (key != NULL)
  {
    ssh_key_free(key);
  }

  limeade_context_free_common(ctx);
}

/*** EXTERNAL FUNCTIONS START HERE ***/

LIMEADE_CONTEXT limeade_host_init(unsigned int port)
{
  LIMEADE_CONTEXT ctx;
  ssh_message msg;
  ssh_key hostkey;
  ssh_channel channel;
  int port_int;
  bool authed;
  bool subsystem_ready;

  ctx = (LIMEADE_CONTEXT)calloc(1, sizeof(*ctx));
  if (ctx == NULL)
  {
    return NULL;
  }

  ctx->role = LIMEADE_ROLE_HOST;
  ctx->timeout_ms = -1;
  ctx->port = port;
  hostkey = NULL;
  channel = NULL;

  ctx->bind = ssh_bind_new();
  ctx->session = ssh_new();
  if (ctx->bind == NULL || ctx->session == NULL)
  {
    limeade_inserror(LIMEADE_ERROR_SSH_INIT);
    limeade_context_cleanup_on_error(ctx, hostkey);
    return NULL;
  }

  port_int = (int)port;
  if (ssh_bind_options_set(ctx->bind, SSH_BIND_OPTIONS_BINDADDR, "0.0.0.0") <
          0 ||
      ssh_bind_options_set(ctx->bind, SSH_BIND_OPTIONS_BINDPORT, &port_int) < 0)
  {
    limeade_inserror(LIMEADE_ERROR_SSH_BIND);
    limeade_context_cleanup_on_error(ctx, hostkey);
    return NULL;
  }

  if (ssh_pki_generate(SSH_KEYTYPE_RSA, 2048, &hostkey) != SSH_OK ||
      ssh_bind_options_set(ctx->bind, SSH_BIND_OPTIONS_IMPORT_KEY, hostkey) < 0)
  {
    limeade_inserror(LIMEADE_ERROR_SSH_BIND);
    limeade_context_cleanup_on_error(ctx, hostkey);
    return NULL;
  }

  if (ssh_bind_listen(ctx->bind) < 0)
  {
    limeade_inserror(LIMEADE_ERROR_SSH_LISTEN);
    limeade_context_cleanup_on_error(ctx, hostkey);
    return NULL;
  }

  if (ssh_bind_accept(ctx->bind, ctx->session) != SSH_OK)
  {
    limeade_inserror(LIMEADE_ERROR_SSH_ACCEPT);
    limeade_context_cleanup_on_error(ctx, hostkey);
    return NULL;
  }

  if (ssh_handle_key_exchange(ctx->session) != SSH_OK)
  {
    limeade_inserror(LIMEADE_ERROR_SSH_KEX);
    limeade_context_cleanup_on_error(ctx, hostkey);
    return NULL;
  }

  authed = false;
  while ((msg = ssh_message_get(ctx->session)) != NULL)
  {
    if (ssh_message_type(msg) == SSH_REQUEST_AUTH &&
        ssh_message_subtype(msg) == SSH_AUTH_METHOD_PUBLICKEY)
    {
      ssh_message_auth_reply_success(msg, 0);
      ssh_message_free(msg);
      authed = true;
      break;
    }

    ssh_message_reply_default(msg);
    ssh_message_free(msg);
  }

  if (!authed)
  {
    limeade_inserror(LIMEADE_ERROR_SSH_AUTH);
    limeade_context_cleanup_on_error(ctx, hostkey);
    return NULL;
  }

  while ((msg = ssh_message_get(ctx->session)) != NULL)
  {
    if (ssh_message_type(msg) == SSH_REQUEST_CHANNEL_OPEN &&
        ssh_message_subtype(msg) == SSH_CHANNEL_SESSION)
    {
      channel = ssh_message_channel_request_open_reply_accept(msg);
      ssh_message_free(msg);
      break;
    }

    ssh_message_reply_default(msg);
    ssh_message_free(msg);
  }

  if (channel == NULL)
  {
    limeade_inserror(LIMEADE_ERROR_SSH_CHANNEL);
    limeade_context_cleanup_on_error(ctx, hostkey);
    return NULL;
  }

  subsystem_ready = false;
  while ((msg = ssh_message_get(ctx->session)) != NULL)
  {
    if (ssh_message_type(msg) == SSH_REQUEST_CHANNEL &&
        ssh_message_subtype(msg) == SSH_CHANNEL_REQUEST_SUBSYSTEM)
    {
      const char *subsystem = ssh_message_channel_request_subsystem(msg);

      if (subsystem != NULL && strcmp(subsystem, LIMEADE_SUBSYSTEM_NAME) == 0)
      {
        ssh_message_channel_request_reply_success(msg);
        ssh_message_free(msg);
        subsystem_ready = true;
        break;
      }
    }

    ssh_message_reply_default(msg);
    ssh_message_free(msg);
  }

  if (!subsystem_ready)
  {
    limeade_inserror(LIMEADE_ERROR_SSH_SUBSYSTEM);
    limeade_context_cleanup_on_error(ctx, hostkey);
    return NULL;
  }

  ctx->channel = channel;
  limeade_generate_session_id(ctx->session_id);
  ctx->has_session_id = true;

  if (hostkey != NULL)
  {
    ssh_key_free(hostkey);
  }

  limeade_inserror(LIMEADE_SUCCESS);
  return ctx;
}

LIMEADE_CONTEXT limeade_client_init(unsigned int port, const char *user,
                                    const char *passwd)
{
  LIMEADE_CONTEXT ctx;
  int port_int;

  (void)passwd;

  ctx = (LIMEADE_CONTEXT)calloc(1, sizeof(*ctx));
  if (ctx == NULL)
  {
    return NULL;
  }

  ctx->role = LIMEADE_ROLE_CLIENT;
  ctx->timeout_ms = -1;
  ctx->port = port;

  ctx->session = ssh_new();
  if (ctx->session == NULL)
  {
    limeade_inserror(LIMEADE_ERROR_SSH_INIT);
    limeade_context_free_common(ctx);
    return NULL;
  }

  port_int = (int)port;
  if (ssh_options_set(ctx->session, SSH_OPTIONS_HOST, "127.0.0.1") < 0 ||
      ssh_options_set(ctx->session, SSH_OPTIONS_PORT, &port_int) < 0 ||
      (user != NULL &&
       ssh_options_set(ctx->session, SSH_OPTIONS_USER, user) < 0))
  {
    limeade_inserror(LIMEADE_ERROR_SSH_INIT);
    limeade_context_free_common(ctx);
    return NULL;
  }

  if (ssh_connect(ctx->session) != SSH_OK)
  {
    limeade_inserror(LIMEADE_ERROR_SSH_INIT);
    limeade_context_free_common(ctx);
    return NULL;
  }

  if (ssh_userauth_publickey_auto(ctx->session, NULL, NULL) != SSH_AUTH_SUCCESS)
  {
    limeade_inserror(LIMEADE_ERROR_SSH_AUTH);
    limeade_context_free_common(ctx);
    return NULL;
  }

  ctx->channel = ssh_channel_new(ctx->session);
  if (ctx->channel == NULL)
  {
    limeade_inserror(LIMEADE_ERROR_SSH_CHANNEL);
    limeade_context_free_common(ctx);
    return NULL;
  }

  if (ssh_channel_open_session(ctx->channel) != SSH_OK)
  {
    limeade_inserror(LIMEADE_ERROR_SSH_CHANNEL);
    limeade_context_free_common(ctx);
    return NULL;
  }

  if (ssh_channel_request_subsystem(ctx->channel, LIMEADE_SUBSYSTEM_NAME) !=
      SSH_OK)
  {
    limeade_inserror(LIMEADE_ERROR_SSH_SUBSYSTEM);
    limeade_context_free_common(ctx);
    return NULL;
  }

  limeade_inserror(LIMEADE_SUCCESS);
  return ctx;
}

int limeade_host_send(LIMEADE_CONTEXT ctx, LIMEADE_PACKET pckt)
{
  return limeade_send_common(ctx, pckt, LIMEADE_ROLE_HOST);
}

int limeade_client_send(LIMEADE_CONTEXT ctx, LIMEADE_PACKET pckt)
{
  return limeade_send_common(ctx, pckt, LIMEADE_ROLE_CLIENT);
}

LIMEADE_PACKET limeade_host_recv(LIMEADE_CONTEXT ctx)
{
  return limeade_recv_common(ctx, LIMEADE_ROLE_HOST);
}

LIMEADE_PACKET limeade_client_recv(LIMEADE_CONTEXT ctx)
{
  return limeade_recv_common(ctx, LIMEADE_ROLE_CLIENT);
}

int limeade_context_set_timeout(LIMEADE_CONTEXT ctx, int timeout_ms)
{
  if (ctx == NULL)
  {
    limeade_inserror(LIMEADE_ERROR_INVALID_CONTEXT);
    return -1;
  }

  ctx->timeout_ms = timeout_ms;
  limeade_inserror(LIMEADE_SUCCESS);
  return 0;
}

void limeade_host_free(LIMEADE_CONTEXT ctx)
{
  if (ctx != NULL && ctx->role != LIMEADE_ROLE_HOST)
  {
    limeade_inserror(LIMEADE_ERROR_INVALID_CONTEXT);
    return;
  }

  limeade_context_free_common(ctx);
}

void limeade_client_free(LIMEADE_CONTEXT ctx)
{
  if (ctx != NULL && ctx->role != LIMEADE_ROLE_CLIENT)
  {
    limeade_inserror(LIMEADE_ERROR_INVALID_CONTEXT);
    return;
  }

  limeade_context_free_common(ctx);
}
