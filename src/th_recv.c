// th_recv.c
// AGPL

#include<errno.h>
#include<netinet/in.h>
#include<poll.h>
#include<pthread.h>
#include<stdlib.h>
#include<stdint.h>
#include<unistd.h>

#include<liblimeade/liblimeade-internal.h>

#define MAGSZ sizeof(LIMEADE_MAGIC)

void *limeade_th_recv_host_eth(void *arg)
{
  struct limeade_context *ctx;
  struct limeade_recv_data *r;
  struct pollfd recv_poll;
  struct sockaddr_in tmp_sockaddr;
  socklen_t tmp_socklen;
  void *buffer;
  int amt_recv, poll_ms;

  ctx = arg;
  r   = &ctx->recv;

  pthread_mutex_lock(&r->ack.mtx);

  poll_ms = 1000/(r->hz);

  buffer = malloc(LIMEADE_MAX_PKT_SZ);
  if(!buffer)
    return NULL;

  recv_poll.events = POLLIN;
  recv_poll.fd = ctx->rfd;

  while(sem_trywait(&r->sem_kys) == EAGAIN)
  {

    if(!poll(&recv_poll, 1, poll_ms) || !(recv_poll.revents & POLLIN))
      continue;

    pthread_mutex_lock(&ctx->mtx_rfd);
    pthread_mutex_lock(&ctx->mtx_mode_specific);
    
    tmp_socklen = sizeof(tmp_sockaddr);
    amt_recv = recvfrom(recv_poll.fd, buffer, LIMEADE_MAX_PKT_SZ, 0,
                        (struct sockaddr*)&tmp_sockaddr, &tmp_socklen);

    pthread_mutex_unlock(&ctx->mtx_rfd);
    pthread_mutex_unlock(&ctx->mtx_mode_specific);

    if(limeade_prelim_confirm(buffer, amt_recv) != 0)
      continue;

    limeade_th_recv_wr_pkt(r, buffer, amt_recv, &tmp_sockaddr, tmp_socklen);
  }
  free(buffer);
  return NULL;
}

void *limeade_th_recv_client_eth(void *arg)
{
  struct limeade_context *ctx;
  struct limeade_recv_data *r;
  struct pollfd recv_poll;
  void *buffer;
  int amt_recv, poll_ms;

  ctx = arg;
  r   = &ctx->recv;
  
  pthread_mutex_lock(&r->ack.mtx);

  poll_ms = 1000/(r->hz);

  buffer = malloc(LIMEADE_MAX_PKT_SZ);
  if(!buffer)
    return NULL;

  recv_poll.events = POLLIN;
  recv_poll.fd     = ctx->rfd;

  while(sem_trywait(&r->sem_kys) == EAGAIN)
  {

    if(!poll(&recv_poll, 1, poll_ms) || !(recv_poll.revents & POLLIN))
      continue;

    pthread_mutex_lock(&ctx->mtx_rfd);

    amt_recv = recvfrom(recv_poll.fd, buffer, LIMEADE_MAX_PKT_SZ, 0, NULL, NULL);

    pthread_mutex_unlock(&ctx->mtx_rfd);

    if(limeade_prelim_confirm(buffer, amt_recv) != 0)
      continue;

    limeade_th_recv_wr_pkt(r, buffer, amt_recv, NULL, 0);

    recv_poll.fd = ctx->rfd;
  }
  free(buffer);
  return NULL;
}


void *limeade_th_recv_client_ssh(void *arg)
{
  struct limeade_context *ctx;
  struct limeade_recv_data *r;
  struct pollfd recv_poll;
  void *buffer;
  int amt_recv, poll_ms;

  ctx = arg;
  r   = &ctx->recv;

  pthread_mutex_lock(&r->ack.mtx);

  poll_ms = 1000/(r->hz);

  buffer = malloc(LIMEADE_MAX_PKT_SZ);
  if(!buffer)
    return NULL;

  recv_poll.events = POLLIN;

  while(sem_trywait(&r->sem_kys) == EAGAIN)
  {
    recv_poll.fd = ctx->rfd;

    if(!poll(&recv_poll, 1, poll_ms) || !(recv_poll.revents & POLLIN))
      continue;

    pthread_mutex_lock(&ctx->mtx_rfd);

    amt_recv = read(recv_poll.fd, buffer, LIMEADE_MAX_PKT_SZ);

    pthread_mutex_unlock(&ctx->mtx_rfd);
    
    if(limeade_prelim_confirm(buffer, amt_recv) != 0)
      continue;

    limeade_th_recv_wr_pkt(r, buffer, amt_recv, NULL, 0);
  }

  free(buffer);
  return NULL;
}

void *limeade_th_recv_host_ssh(void *arg)
{
  return limeade_th_recv_client_ssh(arg);
}

#undef MAGSZ
