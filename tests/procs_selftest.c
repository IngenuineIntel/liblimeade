#include <arpa/inet.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <liblimeade.h>

#define MAGIC_LEN 7
#define FLAGS_LEN 7
#define SESSION_LEN 5
#define FIELD_DELIM '\xFE'
#define ROW_DELIM '\xFF'

typedef struct
{
  TS ts;
  pid_t pid;
  pid_t ppid;
  uid_t uid;
  uint16_t threads;
  uint32_t cpu_ticks;
  uint32_t vm_rss_kb;
  const char *comm;
} TEST_PROC_ROW;

typedef struct
{
  unsigned int port;
  LIMEADE_CONTEXT host_ctx;
  LIMEADE_CONTEXT client_ctx;
  int host_ready;
  int client_ready;
  int init_failed;
  pthread_mutex_t lock;
  pthread_cond_t cv;
} TEST_STATE;

typedef struct
{
  TEST_STATE *state;
  LIMEADE_PACKET sent_packet;
  int send_rc;
} SEND_STATE;

typedef struct
{
  TEST_STATE *state;
  LIMEADE_PACKET received_packet;
} RECV_STATE;

static void packet_free(LIMEADE_PACKET *pkt)
{
  if (pkt != NULL)
  {
    free(pkt->data);
    pkt->data = NULL;
    pkt->sz = 0;
  }
}

static size_t append_uint(char *dst, size_t cap, size_t off, unsigned long long v,
                          char delim)
{
  int n;

  if (off >= cap)
  {
    return 0;
  }

  n = snprintf(dst + off, cap - off, "%llu", v);
  if (n < 0)
  {
    return 0;
  }

  off += (size_t)n;
  if (off + 1 > cap)
  {
    return 0;
  }

  dst[off++] = delim;
  return off;
}

static size_t append_int(char *dst, size_t cap, size_t off, long long v,
                         char delim)
{
  int n;

  if (off >= cap)
  {
    return 0;
  }

  n = snprintf(dst + off, cap - off, "%lld", v);
  if (n < 0)
  {
    return 0;
  }

  off += (size_t)n;
  if (off + 1 > cap)
  {
    return 0;
  }

  dst[off++] = delim;
  return off;
}

static size_t append_str(char *dst, size_t cap, size_t off, const char *s,
                         char delim)
{
  size_t i;

  if (off >= cap)
  {
    return 0;
  }

  if (s == NULL)
  {
    s = "";
  }

  for (i = 0; s[i] != '\0'; i++)
  {
    char ch = s[i];
    if (ch == FIELD_DELIM || ch == ROW_DELIM)
    {
      ch = ' ';
    }
    if (off + 1 > cap)
    {
      return 0;
    }
    dst[off++] = ch;
  }

  if (off + 1 > cap)
  {
    return 0;
  }

  dst[off++] = delim;
  return off;
}

static int build_procs_payload(const TEST_PROC_ROW *rows, size_t nr_rows,
                               unsigned char **out_payload,
                               uint16_t *out_payload_len)
{
  size_t i;
  size_t cap;
  size_t off;
  char *buf;

  if (rows == NULL || out_payload == NULL || out_payload_len == NULL)
  {
    return -1;
  }

  cap = (nr_rows * 128) + 512;
  buf = (char *)malloc(cap);
  if (buf == NULL)
  {
    return -1;
  }

  off = 0;
  for (i = 0; i < nr_rows; i++)
  {
    off = append_uint(buf, cap, off, (unsigned long long)rows[i].ts.s,
                      FIELD_DELIM);
    if (off == 0)
    {
      free(buf);
      return -1;
    }

    off = append_uint(buf, cap, off, (unsigned long long)rows[i].ts.ms,
                      FIELD_DELIM);
    if (off == 0)
    {
      free(buf);
      return -1;
    }

    off = append_int(buf, cap, off, (long long)rows[i].pid, FIELD_DELIM);
    if (off == 0)
    {
      free(buf);
      return -1;
    }

    off = append_int(buf, cap, off, (long long)rows[i].ppid, FIELD_DELIM);
    if (off == 0)
    {
      free(buf);
      return -1;
    }

    off = append_uint(buf, cap, off, (unsigned long long)rows[i].uid,
                      FIELD_DELIM);
    if (off == 0)
    {
      free(buf);
      return -1;
    }

    off = append_uint(buf, cap, off, (unsigned long long)rows[i].threads,
                      FIELD_DELIM);
    if (off == 0)
    {
      free(buf);
      return -1;
    }

    off = append_uint(buf, cap, off, (unsigned long long)rows[i].cpu_ticks,
                      FIELD_DELIM);
    if (off == 0)
    {
      free(buf);
      return -1;
    }

    off = append_uint(buf, cap, off, (unsigned long long)rows[i].vm_rss_kb,
                      FIELD_DELIM);
    if (off == 0)
    {
      free(buf);
      return -1;
    }

    off = append_str(buf, cap, off, rows[i].comm, ROW_DELIM);
    if (off == 0)
    {
      free(buf);
      return -1;
    }
  }

  if (off > 16384)
  {
    free(buf);
    return -1;
  }

  *out_payload = (unsigned char *)buf;
  *out_payload_len = (uint16_t)off;
  return 0;
}

static int build_wire_packet(uint8_t type, const unsigned char *payload,
                             uint16_t payload_len, LIMEADE_PACKET *out)
{
  unsigned char *wire;
  size_t total;
  uint8_t flags[FLAGS_LEN];

  if (payload == NULL || payload_len == 0 || out == NULL)
  {
    return -1;
  }

  total = MAGIC_LEN + FLAGS_LEN + SESSION_LEN + payload_len;
  if (total > UINT32_MAX)
  {
    return -1;
  }

  wire = (unsigned char *)malloc(total);
  if (wire == NULL)
  {
    return -1;
  }

  memcpy(wire, LIMEADE_MAGIC, MAGIC_LEN);

  memset(flags, 0, sizeof(flags));
  flags[0] = (uint8_t)((LIMEADE_PROTOCOL_VERSION.maj << 4) |
                       (LIMEADE_PROTOCOL_VERSION.min & 0x0F));
  flags[1] = (uint8_t)(((type & 0x0F) << 4) | ((payload_len >> 10) & 0x0F));
  flags[2] = (uint8_t)((payload_len >> 2) & 0xFF);
  flags[3] = (uint8_t)((payload_len & 0x03) << 6); /* compressed size = 0 */
  flags[4] = 0;
  flags[5] = (uint8_t)FIELD_DELIM;
  flags[6] = (uint8_t)ROW_DELIM;

  memcpy(wire + MAGIC_LEN, flags, FLAGS_LEN);
  memset(wire + MAGIC_LEN + FLAGS_LEN, 0, SESSION_LEN);
  memcpy(wire + MAGIC_LEN + FLAGS_LEN + SESSION_LEN, payload, payload_len);

  out->data = wire;
  out->sz = (uint32_t)total;
  return 0;
}

static int parse_wire_payload(const LIMEADE_PACKET *pkt,
                              const unsigned char **payload_out,
                              size_t *payload_len_out)
{
  const unsigned char *wire;
  uint8_t b1;
  uint8_t b2;
  uint8_t b3;
  uint8_t b4;
  uint16_t before;
  uint16_t after;
  size_t session_len;
  size_t payload_len;
  size_t min_sz;

  if (pkt == NULL || pkt->data == NULL || payload_out == NULL ||
      payload_len_out == NULL)
  {
    return -1;
  }

  if (pkt->sz < MAGIC_LEN + FLAGS_LEN)
  {
    return -1;
  }

  wire = pkt->data;
  if (memcmp(wire, LIMEADE_MAGIC, MAGIC_LEN) != 0)
  {
    return -1;
  }

  b1 = wire[MAGIC_LEN + 1];
  b2 = wire[MAGIC_LEN + 2];
  b3 = wire[MAGIC_LEN + 3];
  b4 = wire[MAGIC_LEN + 4];

  before = (uint16_t)(b1 & 0x0F);
  before = (uint16_t)((before << 8) | b2);
  before = (uint16_t)((before << 2) | ((b3 >> 6) & 0x03));

  after = (uint16_t)(b3 & 0x3F);
  after = (uint16_t)((after << 8) | b4);

  session_len = (((wire[MAGIC_LEN + 1] >> 4) & 0x0F) ==
                 (uint8_t)LIMEADE_CLIENT_ASK)
                    ? 0
                    : SESSION_LEN;

  payload_len = (after > 0) ? after : before;
  min_sz = MAGIC_LEN + FLAGS_LEN + session_len + payload_len;
  if (pkt->sz < min_sz)
  {
    return -1;
  }

  *payload_out = wire + MAGIC_LEN + FLAGS_LEN + session_len;
  *payload_len_out = payload_len;
  return 0;
}

static void display_process_payload(const unsigned char *payload, size_t len)
{
  char *tmp;
  size_t i;
  size_t token_start;
  size_t row_no;
  size_t field_no;

  if (payload == NULL || len == 0)
  {
    fprintf(stderr, "No process payload to display.\n");
    return;
  }

  tmp = (char *)malloc(len + 1);
  if (tmp == NULL)
  {
    fprintf(stderr, "malloc failed while displaying payload\n");
    return;
  }

  memcpy(tmp, payload, len);
  tmp[len] = '\0';

  printf("Decoded process rows:\n");
  printf("ts_s | ts_ms | pid | ppid | uid | threads | cpu_ticks | vm_rss_kb | comm\n");
  printf("-----+-------+-----+------+-----+---------+-----------+-----------+-----\n");

  token_start = 0;
  row_no = 0;
  field_no = 0;
  for (i = 0; i < len; i++)
  {
    unsigned char ch = (unsigned char)tmp[i];

    if (ch == (unsigned char)FIELD_DELIM || ch == (unsigned char)ROW_DELIM)
    {
      tmp[i] = '\0';
      if (field_no > 0)
      {
        printf(" | ");
      }
      printf("%s", tmp + token_start);

      field_no++;
      token_start = i + 1;

      if (ch == (unsigned char)ROW_DELIM)
      {
        printf("\n");
        row_no++;
        field_no = 0;
      }
    }
  }

  printf("Displayed %zu process row(s).\n", row_no);
  free(tmp);
}

static void *host_init_thread(void *arg)
{
  TEST_STATE *state = (TEST_STATE *)arg;
  LIMEADE_CONTEXT ctx;

  ctx = limeade_host_init(state->port);

  pthread_mutex_lock(&state->lock);
  state->host_ctx = ctx;
  state->host_ready = 1;
  if (ctx == NULL)
  {
    state->init_failed = 1;
  }
  pthread_cond_broadcast(&state->cv);
  pthread_mutex_unlock(&state->lock);

  return NULL;
}

static void *client_init_thread(void *arg)
{
  TEST_STATE *state = (TEST_STATE *)arg;
  LIMEADE_CONTEXT ctx;
  struct timespec ts;

  /* Give the host thread a brief chance to bind/listen first. */
  ts.tv_sec = 0;
  ts.tv_nsec = 150000000;
  nanosleep(&ts, NULL);
  ctx = limeade_client_init(state->port, NULL, NULL);

  pthread_mutex_lock(&state->lock);
  state->client_ctx = ctx;
  state->client_ready = 1;
  if (ctx == NULL)
  {
    state->init_failed = 1;
  }
  pthread_cond_broadcast(&state->cv);
  pthread_mutex_unlock(&state->lock);

  return NULL;
}

static void *send_thread(void *arg)
{
  SEND_STATE *send_state = (SEND_STATE *)arg;
  TEST_STATE *state = send_state->state;
  TEST_PROC_ROW rows[2];
  unsigned char *payload;
  uint16_t payload_len;
  LIMEADE_PACKET pkt;

  payload = NULL;
  payload_len = 0;
  pkt.sz = 0;
  pkt.data = NULL;

  rows[0].ts.s = 1721000001;
  rows[0].ts.ms = 111;
  rows[0].pid = 4201;
  rows[0].ppid = 1;
  rows[0].uid = getuid();
  rows[0].threads = 4;
  rows[0].cpu_ticks = 10240;
  rows[0].vm_rss_kb = 78124;
  rows[0].comm = "selftest-agent-a";

  rows[1].ts.s = 1721000002;
  rows[1].ts.ms = 222;
  rows[1].pid = 4202;
  rows[1].ppid = 4201;
  rows[1].uid = getuid();
  rows[1].threads = 2;
  rows[1].cpu_ticks = 20480;
  rows[1].vm_rss_kb = 50912;
  rows[1].comm = "selftest-agent-b";

  if (build_procs_payload(rows, 2, &payload, &payload_len) != 0)
  {
    send_state->send_rc = -1;
    return NULL;
  }

  if (build_wire_packet((uint8_t)LIMEADE_CLIENT_PROCS_GENERIC, payload,
                        payload_len, &pkt) != 0)
  {
    free(payload);
    send_state->send_rc = -1;
    return NULL;
  }

  free(payload);

  send_state->sent_packet = pkt;
  send_state->send_rc = limeade_host_send(state->host_ctx, pkt);
  return NULL;
}

static void *recv_thread(void *arg)
{
  RECV_STATE *recv_state = (RECV_STATE *)arg;

  recv_state->received_packet = limeade_client_recv(recv_state->state->client_ctx);
  return NULL;
}

int main(void)
{
  TEST_STATE state;
  SEND_STATE send_state;
  RECV_STATE recv_state;
  pthread_t th_host;
  pthread_t th_client;
  pthread_t th_send;
  pthread_t th_recv;
  int rc;
  const unsigned char *payload;
  size_t payload_len;

  memset(&state, 0, sizeof(state));
  memset(&send_state, 0, sizeof(send_state));
  memset(&recv_state, 0, sizeof(recv_state));

  state.port = 43222;
  pthread_mutex_init(&state.lock, NULL);
  pthread_cond_init(&state.cv, NULL);

  send_state.state = &state;
  recv_state.state = &state;

  rc = pthread_create(&th_host, NULL, host_init_thread, &state);
  if (rc != 0)
  {
    fprintf(stderr, "failed to start host init thread\n");
    return 1;
  }

  rc = pthread_create(&th_client, NULL, client_init_thread, &state);
  if (rc != 0)
  {
    fprintf(stderr, "failed to start client init thread\n");
    return 1;
  }

  pthread_join(th_host, NULL);
  pthread_join(th_client, NULL);

  if (state.init_failed || state.host_ctx == NULL || state.client_ctx == NULL)
  {
    fprintf(stderr, "failed to establish loopback SSH contexts\n");
    limeade_host_free(state.host_ctx);
    limeade_client_free(state.client_ctx);
    return 2;
  }

  printf("Loopback contexts initialized on port %u.\n", state.port);

  rc = pthread_create(&th_recv, NULL, recv_thread, &recv_state);
  if (rc != 0)
  {
    fprintf(stderr, "failed to start recv thread\n");
    limeade_host_free(state.host_ctx);
    limeade_client_free(state.client_ctx);
    return 3;
  }

  rc = pthread_create(&th_send, NULL, send_thread, &send_state);
  if (rc != 0)
  {
    fprintf(stderr, "failed to start send thread\n");
    limeade_host_free(state.host_ctx);
    limeade_client_free(state.client_ctx);
    return 3;
  }

  pthread_join(th_send, NULL);
  pthread_join(th_recv, NULL);

  if (send_state.send_rc < 0)
  {
    fprintf(stderr, "host send failed\n");
    packet_free(&send_state.sent_packet);
    packet_free(&recv_state.received_packet);
    limeade_host_free(state.host_ctx);
    limeade_client_free(state.client_ctx);
    return 4;
  }

  if (recv_state.received_packet.data == NULL || recv_state.received_packet.sz == 0)
  {
    fprintf(stderr, "client recv failed\n");
    packet_free(&send_state.sent_packet);
    packet_free(&recv_state.received_packet);
    limeade_host_free(state.host_ctx);
    limeade_client_free(state.client_ctx);
    return 5;
  }

  printf("Sent %d bytes and received %u bytes.\n", send_state.send_rc,
         recv_state.received_packet.sz);

  if (parse_wire_payload(&recv_state.received_packet, &payload, &payload_len) != 0)
  {
    fprintf(stderr, "failed to parse received process payload\n");
    packet_free(&send_state.sent_packet);
    packet_free(&recv_state.received_packet);
    limeade_host_free(state.host_ctx);
    limeade_client_free(state.client_ctx);
    return 6;
  }

  display_process_payload(payload, payload_len);

  packet_free(&send_state.sent_packet);
  packet_free(&recv_state.received_packet);
  limeade_host_free(state.host_ctx);
  limeade_client_free(state.client_ctx);

  pthread_cond_destroy(&state.cv);
  pthread_mutex_destroy(&state.lock);

  printf("Self-loopback process-data test completed successfully.\n");
  return 0;
}
