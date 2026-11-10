#include <arpa/inet.h>
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdatomic.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include <liblimeade/liblimeade.h>

#define SENDER_COUNT 4
#define SENDS_PER_SENDER 12
#define TOTAL_SENDS (SENDER_COUNT * SENDS_PER_SENDER)

struct sender_args
{
  struct limeade_context *ctx;
  atomic_int *failures;
};

struct receiver_args
{
  struct limeade_context *ctx;
  atomic_int *failures;
};

static void *send_packets(void *arg)
{
  struct sender_args *args = arg;
  struct limeade_perf packet = {0};

  for(int i = 0; i < SENDS_PER_SENDER; i++)
  {
    int ret = limeade_send_await(args->ctx, LIMEADE_PACKET_PERF, packet);
    if(ret != LIMEADE_SUCCESS)
    {
      fprintf(stderr, "sender failed: %s\n", LIMEADE_ERROR_REPRS[ret]);
      atomic_fetch_add(args->failures, 1);
      break;
    }
  }
  return NULL;
}

static void *receive_packets(void *arg)
{
  struct receiver_args *args = arg;
  const struct timespec delay = {0, 5000000};

  for(int i = 0; i < TOTAL_SENDS; i++)
  {
    struct limeade_recvd packet = {0};
    int ret = limeade_recv_wait(args->ctx, &packet, 5000);
    if(ret != LIMEADE_SUCCESS)
    {
      fprintf(stderr, "receiver failed: %s\n", LIMEADE_ERROR_REPRS[ret]);
      atomic_fetch_add(args->failures, 1);
      return NULL;
    }

    nanosleep(&delay, NULL);
    limeade_release(&packet);
  }
  return NULL;
}

static int find_available_port(void)
{
  struct sockaddr_in addr = {0};
  socklen_t addr_len = sizeof(addr);
  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  assert(fd >= 0);

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = 0;
  assert(bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == 0);
  assert(getsockname(fd, (struct sockaddr*)&addr, &addr_len) == 0);
  close(fd);
  return ntohs(addr.sin_port);
}

int main(void)
{
  struct limeade_context host, client;
  struct sender_args sender;
  struct receiver_args receiver;
  pthread_t senders[SENDER_COUNT], receiver_tid;
  atomic_int failures;
  int port = find_available_port();
  atomic_init(&failures, 0);
  sender = (struct sender_args){&client, &failures};
  receiver = (struct receiver_args){&host, &failures};

  assert(limeade_init(&host, LIMEADE_MODE_HOST_ETH | LIMEADE_MODE_NO_COMPRESSION,
                      port) == LIMEADE_SUCCESS);
  assert(limeade_init(&client, LIMEADE_MODE_CLIENT_ETH | LIMEADE_MODE_NO_COMPRESSION,
                      "127.0.0.1", port) == LIMEADE_SUCCESS);
  assert(limeade_connect(&host) == LIMEADE_SUCCESS);
  assert(limeade_connect(&client) == LIMEADE_SUCCESS);
  client.ack_wait_time_ms = 3000;

  assert(pthread_create(&receiver_tid, NULL, receive_packets, &receiver) == 0);
  for(int i = 0; i < SENDER_COUNT; i++)
    assert(pthread_create(&senders[i], NULL, send_packets, &sender) == 0);

  for(int i = 0; i < SENDER_COUNT; i++)
    pthread_join(senders[i], NULL);
  pthread_join(receiver_tid, NULL);

  int failure_count = atomic_load(sender.failures);
  limeade_destruct(&client);
  limeade_destruct(&host);
  assert(failure_count == 0);
  return 0;
}
