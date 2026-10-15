#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <liblimeade/liblimeade.h>

#define NR_SAMPLES 1000
#define NR_PROCS 512
#define OUT_CSV "test4.csv"

#define STOPWATCH(a) clock_gettime(CLOCK_MONOTONIC, &a[i])
#define TEST(e)                                                                \
  if (e != LIMEADE_OK)                                                         \
  {                                                                            \
    printf("err: %s\n", LIMEADE_ERROR_REPRS[e]);                                 \
    goto end;                                                                  \
  }

static int64_t ts_diff(struct timespec start, struct timespec stop)
{
  return (stop.tv_sec - start.tv_sec) * 1000000000LL +
         (stop.tv_nsec - start.tv_nsec);
}

static int proc_generic_matches(const struct limeade_proc_generic *a,
                                const struct limeade_proc_generic *b)
{
  if (a->ts_s != b->ts_s || a->ts_ms != b->ts_ms || a->total != b->total)
    return 0;

  for (int i = 0; i < a->total; i++)
  {
    const struct limeade_indiv_proc *x = &a->procs[i];
    const struct limeade_indiv_proc *y = &b->procs[i];

    if (x->pid != y->pid || x->ppid != y->ppid || x->ruid != y->ruid ||
        x->euid != y->euid || x->suid != y->suid || x->fsuid != y->fsuid ||
        x->rgid != y->rgid || x->egid != y->egid || x->sgid != y->sgid ||
        x->fsgid != y->fsgid || x->threads != y->threads ||
        x->cpu_total != y->cpu_total || x->cpu_user != y->cpu_user ||
        x->cpu_sys != y->cpu_sys || x->ram_kb != y->ram_kb ||
        x->swap_kb != y->swap_kb || x->state != y->state ||
        strcmp(x->command, y->command) != 0)
      return 0;
  }

  return 1;
}

int main(void)
{
  struct timespec init_start[NR_SAMPLES], init_stop[NR_SAMPLES];
  struct timespec conn_start[NR_SAMPLES], conn_stop[NR_SAMPLES];
  struct timespec send_start[NR_SAMPLES], send_stop[NR_SAMPLES];
  struct timespec recv_start[NR_SAMPLES], recv_stop[NR_SAMPLES];
  struct timespec parse_start[NR_SAMPLES], parse_stop[NR_SAMPLES];
  struct timespec release_start[NR_SAMPLES], release_stop[NR_SAMPLES];
  struct timespec rqtp = {0, 500000}, rmtp; // 0.5ms(p)
  struct limeade_context *client = calloc(1, sizeof(*client));
  struct limeade_context *host = calloc(1, sizeof(*host));
  struct limeade_proc_generic pkt1 = {0};
  int host_initialized = 0;
  int client_initialized = 0;
  int e = LIMEADE_ERROR_OTHER;
  int result = 1;

  if (!client || !host)
  {
    fprintf(stderr, "context allocation failed\n");
    goto end;
  }

  pkt1.ts_s = 0x12345678;
  pkt1.ts_ms = 54321;
  pkt1.total = NR_PROCS;
  pkt1.procs = calloc(NR_PROCS, sizeof(*pkt1.procs));
  if (!pkt1.procs)
  {
    fprintf(stderr, "process allocation failed\n");
    goto end;
  }

  for (int i = 0; i < NR_PROCS; i++)
  {
    nanosleep(&rqtp, &rmtp);

    static const char states[] = "RSDTtZXI";
    char name[256];

    snprintf(name, sizeof(name), "limeade-generic-%03d-%s-%s-%s", i,
             "0123456789abcdef", "fedcba9876543210", "abcdefghijklmnop");
    pkt1.procs[i].pid = 1000 + i;
    pkt1.procs[i].ppid = 2000 + i;
    pkt1.procs[i].ruid = 3000 + i;
    pkt1.procs[i].euid = 4000 + i;
    pkt1.procs[i].suid = 5000 + i;
    pkt1.procs[i].fsuid = 6000 + i;
    pkt1.procs[i].rgid = 7000 + i;
    pkt1.procs[i].egid = 8000 + i;
    pkt1.procs[i].sgid = 9000 + i;
    pkt1.procs[i].fsgid = 10000 + i;
    pkt1.procs[i].threads = 1 + (i % 4);
    pkt1.procs[i].cpu_total = i % 101;
    pkt1.procs[i].cpu_user = (i * 3) % 101;
    pkt1.procs[i].cpu_sys = (i * 7) % 101;
    pkt1.procs[i].ram_kb = 4096 + i;
    pkt1.procs[i].swap_kb = 1024 + i;
    pkt1.procs[i].state = states[i % (sizeof(states) - 1)];
    pkt1.procs[i].command = strdup(name);
    if (!pkt1.procs[i].command)
    {
      fprintf(stderr, "command allocation failed\n");
      goto end;
    }
  }

  e = limeade_init(host, LIMEADE_MODE_HOST_ETH | LIMEADE_MODE_NO_COMPRESSION,
                   LIMEADE_PORT);
  TEST(e)

  host_initialized = 1;

  e = limeade_connect(host);
  TEST(e)

  for (int i = 0; i < NR_SAMPLES; i++)
  {
    printf("%i\n", i);
    struct limeade_recvd recvd = {0};
    struct limeade_proc_generic parsed = {0};

    memset(client, 0, sizeof(*client));

    STOPWATCH(init_start);
    e = limeade_init(client,
                     LIMEADE_MODE_CLIENT_ETH | LIMEADE_MODE_LOW_COMPRESSION,
                     "127.0.0.1", LIMEADE_PORT);
    STOPWATCH(init_stop);
    TEST(e)
    client_initialized = 1;
    nanosleep(&rqtp, &rmtp);

    STOPWATCH(conn_start);
    e = limeade_connect(client);
    STOPWATCH(conn_stop);
    TEST(e)
    nanosleep(&rqtp, &rmtp);

    STOPWATCH(send_start);
    e = limeade_send(client, LIMEADE_PACKET_PROC_GENERIC, pkt1);
    STOPWATCH(send_stop);
    TEST(e)
    nanosleep(&rqtp, &rmtp);

    STOPWATCH(recv_start);
    e = limeade_recv_wait_noreply(host, &recvd, 5000);
    STOPWATCH(recv_stop);
    TEST(e)
    nanosleep(&rqtp, &rmtp);

    STOPWATCH(parse_start);
    e = limeade_parse_proc_generic(&parsed, &recvd);
    STOPWATCH(parse_stop);
    TEST(e)
    nanosleep(&rqtp, &rmtp);

    if (!proc_generic_matches(&pkt1, &parsed))
    {
      fprintf(stderr, "process generic round-trip mismatch at sample %d\n", i);
      limeade_release(&parsed);
      limeade_release(&recvd);
      limeade_destruct(client);
      client_initialized = 0;
      goto end;
    }

    STOPWATCH(release_start);
    limeade_release(&parsed);
    limeade_release(&recvd);
    limeade_release(client);
    STOPWATCH(release_stop);
    client_initialized = 0;
  }

  FILE *fd = fopen(OUT_CSV, "w");
  if (!fd)
  {
    perror("open test4.csv");
    goto end;
  }

  fprintf(fd, "limeade_init,limeade_connect,limeade_send,limeade_recv,limeade_"
              "parse,limeade_release,\n");
  for (int i = 0; i < NR_SAMPLES; i++)
  {
    fprintf(fd, "%lld,%lld,%lld,%lld,%lld,%lld,\n",
            (long long)ts_diff(init_start[i], init_stop[i]),
            (long long)ts_diff(conn_start[i], conn_stop[i]),
            (long long)ts_diff(send_start[i], send_stop[i]),
            (long long)ts_diff(recv_start[i], recv_stop[i]),
            (long long)ts_diff(parse_start[i], parse_stop[i]),
            (long long)ts_diff(release_start[i], release_stop[i]));
  }
  if (fclose(fd) != 0)
  {
    perror("write test4.csv");
    goto end;
  }

  printf("Benchmark data written to %s\n", OUT_CSV);
  result = 0;

end:
  if (client_initialized)
    limeade_destruct(client);
  if (host_initialized)
    limeade_destruct(host);
  if (pkt1.procs)
    for (int i = 0; i < NR_PROCS; i++)
      free(pkt1.procs[i].command);
  free(pkt1.procs);
  free(client);
  free(host);
  return result;
}
