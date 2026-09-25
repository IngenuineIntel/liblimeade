// th_recv.c
// AGPL

#include<errno.h>
#include<netinet/in.h>
#include<pthread.h>
#include<stdlib.h>
#include<stdint.h>
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
  struct timespec iter_wait, recv_wait, rmtp;
  void *buffer, *counter, *mag;
  unsigned int amt_recv, t_amt_recv, rfd;

  ctx = arg;
  r   = &ctx->recv;

  pthread_mutex_lock(r->mtx_ack);

  iter_wait.tv_sec  = 0;
  iter_wait.tv_nsec = 999999999/(r->hz ? r->hz > 0 : LIMEADE_RECV_DEFAULT_HZ);

  // 10µs (100,000hz)
  recv_wait.tv_sec  = 0;
  recv_wait.tv_nsec = 10000;

  buffer = malloc(LIMEADE_RECV_TMP_SZ);
  if(!buffer)
    return NULL;

  while(pthread_mutex_trylock(r->mtx_kys) == EBUSY)
  {
    
    pthread_mutex_lock(ctx->mtx_rfd);
    rfd = ctx->rfd;
    t_amt_recv = 0;
    for(;;)
    {
      amt_recv = read(rfd, buffer + t_amt_recv, LIMEADE_RECV_TMP_SZ - t_amt_recv);
      if(amt_recv <= 0)
      {
        if(amt_recv != 0 && errno != EAGAIN && errno != EWOULDBLOCK)
          goto err;
        // simply no data to read
        break;
      }
      t_amt_recv += amt_recv;
      nanosleep(&recv_wait, &rmtp);
    }
    pthread_mutex_unlock(ctx->mtx_rfd);

    counter = buffer;

    while(pthread_mutex_trylock(r->mtx_kys) == EBUSY)
    {
      mag = memmem(counter, t_amt_recv, LIMEADE_MAGIC, MAGSZ);
      if(!mag || mag + MAGSZ >= counter + t_amt_recv)
        break;

      mag        += MAGSZ;
      t_amt_recv -= (mag - counter);
      counter     = mag;

      // TODO maybe, like... a counter for bad packets & an acceptable rate for
      // bad data?
      if(limeade_prelim_confirm(mag - MAGSZ, t_amt_recv) != 0)
        continue;

      limeade_th_recv_wr_pkt(r, mag, t_amt_recv, NULL, 0);
    }

    nanosleep(&iter_wait, &rmtp);
  }

err:
  free(buffer);
  return NULL;
}

void *limeade_th_recv_host_ssh(void *arg)
{
  return limeade_th_recv_client_ssh(arg);
}

#undef MAGSZ
