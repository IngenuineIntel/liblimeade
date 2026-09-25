// th_recv.c
// AGPL

#include<errno.h>
#include<netinet/in.h>
#include<poll.h>
#include<pthread.h>
#include<stdlib.h>
#include<stdint.h>
#include<string.h>
#include<unistd.h>

#include<liblimeade/liblimeade-internal.h>

#define MAGSZ sizeof(LIMEADE_MAGIC)

void *limeade_th_recv_host_eth(void *arg)
{
  /* TODO fancy ass-docstring */
  struct limeade_context *ctx;
  struct limeade_recv_data *r;
  struct timespec iter_wait, rmtp;
  struct sockaddr_in tmp_cliaddr;
  socklen_t tmp_cliaddr_l;
  void *buffer;
  unsigned int amt_recv;

  ctx = arg;
  r = &ctx->recv;

  pthread_mutex_lock(r->mtx_ack);

  memset(&tmp_cliaddr, 0, sizeof(tmp_cliaddr));

  iter_wait.tv_sec  = 0;
  iter_wait.tv_nsec = 999999999/(r->hz ? r->hz > 0 : LIMEADE_RECV_DEFAULT_HZ);

  buffer = malloc(LIMEADE_RECV_TMP_SZ);
  if(!buffer)
    return NULL;

  while(pthread_mutex_trylock(r->mtx_kys) == EBUSY)
  {
    amt_recv = limeade_eth_recv(ctx, buffer, LIMEADE_RECV_TMP_SZ, (struct sockaddr*)&tmp_cliaddr,
                                &tmp_cliaddr_l);

    if(amt_recv <= 0)
    {
      // 0, EWOULDBLOCK, & EAGAIN are intended POSIX standard behavior for an
      // empty socket queue.
      if(amt_recv != 0 && errno != EWOULDBLOCK && errno != EAGAIN)
        break;

      nanosleep(&iter_wait, &rmtp);
      continue;
    }

    if(limeade_prelim_confirm(buffer, amt_recv) != 0)
      continue;

    limeade_th_recv_wr_pkt(r, buffer + MAGSZ, amt_recv,
                          &tmp_cliaddr, tmp_cliaddr_l); 

  }

  free(buffer);
  pthread_mutex_unlock(r->ack);
  return NULL;
}

void *limeade_th_recv_client_eth(void *arg)
{
  struct limeade_context *ctx;
  struct limeade_recv_data *r;
  struct timespec iter_wait, rmtp;
  void *buffer;
  unsigned int amt_recv;

  ctx = arg;
  r   = &ctx->recv;

  pthread_mutex_lock(r->mtx_ack);

  iter_wait.tv_sec  = 0;
  iter_wait.tv_nsec = 999999999/(r->hz ? r->hz > 0 : LIMEADE_RECV_DEFAULT_HZ);

  buffer = malloc(LIMEADE_RECV_TMP_SZ);
  if(!buffer)
    return NULL;

  while(pthread_mutex_trylock(r->mtx_kys) == EBUSY)
  {
    amt_recv = limeade_eth_recv(ctx, buffer, LIMEADE_RECV_TMP_SZ, NULL, 0);

    if(amt_recv <= 0)
    {
      if(amt_recv != 0 && errno != EWOULDBLOCK && errno != EAGAIN)
        break;

      nanosleep(&iter_wait, &rmtp);
      continue;
    }

    if(limeade_prelim_confirm(buffer, amt_recv) != 0)
      continue;

    limeade_th_recv_wr_pkt(r, buffer + MAGSZ, amt_recv, NULL, 0);
  }
  free(buffer);
  pthread_mutex_unlock(r->ack);
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

  pthread_mutex_lock(r->mtx_ack);

  poll_ms = 1000/(r->hz ? r->hz > 0 : LIMEADE_RECV_DEFAULT_HZ);

  buffer = malloc(LIMEADE_RECV_TMP_SZ);
  if(!buffer)
    return NULL;

  recv_poll.events = POLLIN;

  while(pthread_mutex_trylock(r->mtx_kys) == EBUSY)
  {
    recv_poll.fd = ctx->rfd;

    if(!poll(&recv_poll, 1, poll_ms) || !(recv_poll.revents & POLLIN))
      continue;

    pthread_mutex_lock(ctx->mtx_rfd);
    amt_recv = read(recv_poll.fd, buffer, LIMEADE_RECV_TMP_SZ);
    pthread_mutex_unlock(ctx->mtx_rfd);
    
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
