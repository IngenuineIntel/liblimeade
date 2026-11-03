#include<arpa/inet.h>
#include<pthread.h>
#include<stdio.h>
#include<string.h>
#include<sys/socket.h>

#include<liblimeade/liblimeade.h>

struct receive_task
{
  struct limeade_context *ctx;
  int result;
};

static void *receive_one(void *arg)
{
  struct receive_task *task = arg;
  struct limeade_recvd packet = {0};
  task->result = limeade_recv_wait(task->ctx, &packet, 5000);
  if(task->result == LIMEADE_SUCCESS)
    limeade_release_recvd(&packet);
  return NULL;
}

int main(void)
{
  struct limeade_context host = {0}, client = {0};
  struct sockaddr_in bound_addr;
  socklen_t bound_len = sizeof(bound_addr);
  struct receive_task task = {.ctx = &host};
  pthread_t receiver;

  if(limeade_init(&host,
       LIMEADE_MODE_HOST_ETH | LIMEADE_MODE_NO_COMPRESSION, 0)
     != LIMEADE_SUCCESS || limeade_connect(&host) != LIMEADE_SUCCESS)
    return 1;
  if(getsockname(host.rfd, (struct sockaddr*)&bound_addr, &bound_len) != 0)
  {
    limeade_destruct(&host);
    return 1;
  }
  if(limeade_init(&client,
       LIMEADE_MODE_CLIENT_ETH | LIMEADE_MODE_NO_COMPRESSION,
       "127.0.0.1", ntohs(bound_addr.sin_port)) != LIMEADE_SUCCESS
     || limeade_connect(&client) != LIMEADE_SUCCESS)
  {
    limeade_destruct(&host);
    return 1;
  }
  client.ack_wait_time_ms = 5000;
  if(pthread_create(&receiver, NULL, receive_one, &task) != 0)
  {
    limeade_destruct(&client);
    limeade_destruct(&host);
    return 1;
  }

  struct limeade_close close_data = {.explanation = "local reproducer"};
  (void)limeade_send_await(&client, LIMEADE_PACKET_CLOSE, close_data);
  pthread_join(receiver, NULL);
  printf("host receive result: %d\n", task.result);

  limeade_destruct(&client);
  limeade_destruct(&host);
  return 0;
}