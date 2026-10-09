// test 
// the test that is the third

#include<pthread.h>
#include<semaphore.h>
#include<stdio.h>
#include<stdlib.h>
#include<time.h>

#include<liblimeade/liblimeade.h>

#define STOPWATCH(a) clock_gettime(CLOCK_MONOTONIC, &a[i])
#define TEST(e) if(e != LIMEADE_OK){printf("error: %s\n", LIMEADE_ERROR_REPRS[e]); goto end;}
#define NR_SAMPLES 5
#define OUT_CSV    "test3.csv"
#define TS_DIFF(start, stop)\
  (uint64_t)((stop.tv_sec - start.tv_sec) * 1000 + (stop.tv_nsec - stop.tv_nsec) / 1000)

int main()
{
  struct timespec init_start[NR_SAMPLES], init_stop[NR_SAMPLES];
  struct timespec conn_start[NR_SAMPLES], conn_stop[NR_SAMPLES];
  struct timespec send_start[NR_SAMPLES], send_stop[NR_SAMPLES];
  struct timespec recv_start[NR_SAMPLES], recv_stop[NR_SAMPLES];
  struct timespec dstr_start[NR_SAMPLES], dstr_stop[NR_SAMPLES];
  struct limeade_context *ctx  = malloc(sizeof(struct limeade_context));
  struct limeade_context *host = malloc(sizeof(struct limeade_context));
  struct timespec rqtp = {0, 1000000}, rmtp;
  struct limeade_recvd r;
  struct limeade_intro intro = {
    .hostname = "hostname",
    .kernelver = "7.2.9-arch1",
    .distro    = "Arch btw",
    .origin_user = "archuserbtw",
    .processor   = "a distributed array of Intel celerons from '04",
    .vendor      = "DisengenuousIntel",
    .ram_mbs     = 1024 * 1024 * 24,
    .swap_mbs    = 1024 * 1024 * 24,
  };
  int e;

  printf("sizeof(struct limeade_context)      = %lu\n", sizeof(struct limeade_context));
  printf("sizeof(struct limeade_recv_data)    = %lu\n", sizeof(struct limeade_recv_data));
  printf("sizeof(struct limeade_indiv_recv)   = %lu\n", sizeof(struct limeade_indiv_recv));
  printf("sizeof(struct limeade_recvd)        = %lu\n", sizeof(struct limeade_recvd));
  printf("sizeof(struct limeade_packet_flags) = %lu\n", sizeof(struct limeade_packet_flags));
  printf("sizeof(pthread_mutex_t)             = %lu\n", sizeof(pthread_mutex_t));
  printf("sizeof(sem_t)                       = %lu\n", sizeof(sem_t));
  printf("sizeof(int)                         = %lu\n", sizeof(int));
  printf("sizeof(long)                        = %lu\n", sizeof(long));
  printf("sizeof(long long)                   = %lu\n", sizeof(long long));

  e = limeade_init(host, LIMEADE_MODE_HOST_ETH | LIMEADE_MODE_LOW_COMPRESSION, LIMEADE_PORT);
  TEST(e);

  for(int i = 0; i < NR_SAMPLES; i++)
  {
    printf("init...\n");
    STOPWATCH(init_start);
    e = limeade_init(ctx, LIMEADE_MODE_CLIENT_ETH | LIMEADE_MODE_LOW_COMPRESSION, "127.0.0.1", LIMEADE_PORT);
    STOPWATCH(init_stop);
    TEST(e);
    printf("connect...\n");
    STOPWATCH(conn_start);
    e = limeade_connect(ctx);
    STOPWATCH(conn_stop);
    TEST(e);
    printf("sending...\n");
    STOPWATCH(send_start);
    e = limeade_send(ctx, LIMEADE_PACKET_INTRO, intro);
    STOPWATCH(send_stop);
    TEST(e);
    nanosleep(&rqtp, &rmtp);
    printf("receiving...\n");
    STOPWATCH(recv_start);
    e = limeade_recv_wait_noreply(ctx, &r, 5000);
    STOPWATCH(recv_stop);
    TEST(e);
    printf("releasing...\n");
    STOPWATCH(dstr_start);
    limeade_release(ctx);
    limeade_release(&r);
    STOPWATCH(dstr_stop);
  }

  FILE *fd = fopen(OUT_CSV, "w");
  fprintf(fd, "init time,conn time,send time,recv time,destruct time,\n");

  for(int i = 0; i < NR_SAMPLES; i++)
  {
    fprintf(fd, "%lu,%lu,%lu,%lu,%lu,\n", 
            TS_DIFF(init_start[i], init_stop[i]),
            TS_DIFF(conn_start[i], conn_stop[i]),
            TS_DIFF(send_start[i], send_stop[i]),
            TS_DIFF(recv_start[i], recv_stop[i]),
            TS_DIFF(dstr_start[i], dstr_stop[i]));
  }
  fclose(fd);

end:
  free(ctx);
  free(host);
  return 0;
}
