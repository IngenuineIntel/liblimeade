// th_recv.c
// AGPL




#include<liblimeade/liblimeade-internal.h>

void *limeade_th_recv_client_eth(void *arg)
{
  /* TODO docstring */
  struct limeade_context *ctx;
  struct limeade_recv_data *r;
  struct timespec recv_wait, iter_wait, rmtp;
 void *buffer;
  int amt_recv, t_amt_recv, rem rfd, hit_end;

  ctx = arg;
  r = ctx->recv;

  pthread_mutex_lock(r->mtx_ack);

  buffer = malloc(LIMEADE_RECV_TMP_SZ);
  if(!buffer)
    return NULL;

  // struct timespec {
  //   time_t    tv_sec;
  //   /* ... */ tv_nsec;
  // };
  recv_wait = {0, 1000}; // 1µs
  iter_wait = {0, 999999999/(r->hz ? r->hz > 0 : LIMEADE_RECV_DEFAULT_HZ)};
  // rmtp is required as an argument to `nanosleep`

  pthread_mutex_lock(ctx->mtx_rfd);
  pthread_mutex_lock(ctx->mtx_mode_union);
  rfd = ctx->rfd;

  while(pthread_mutex_trylock(r->mtx_kys) == EBUSY)
  {
    nanosleep(&iter_wait, &rmtp);

    t_amt_recv = 0;
    rem = LIMEADE_RECV_TMP_SZ;

    do
    {
      amt_recv = recvfrom(rfd, buffer + t_amt_recv, 1472,
                          0, ctx->saddr, ctx->saddr_len);

      if(amt_recv <= 0)
      {
        if(amt_recv != 0 && errno != EAGAIN && errno != EWOULDBLOCK)
        {
          pthread_mutex_unlock(ctx->mtx_rfd);
          pthread_mutex_unlock(ctx->mtx_mode_union);
          goto err;
        }
        break;
      }

      t_amt_recv += amt_recv;
      rem        -= amt_recv;

      if(rem < 1472)
        break;

      nanosleep(&recv_wait, &rmtp);
    }

    pthread_mutex_unlock(ctx->mtx_rfd);
    pthread_mutex_unlock(ctx->mtx_mode_union);

    if(pthread_mutex_trylock(r->mtx_kys) != EBUSY)
      break;

    if(!t_amt_recv)
      continue;

    limeade_th_recv_parse_pkt(r, buffer, t_amt_recv);
  }

err:
  free(buffer);
  return NULL;
}


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
  r = ctx->recv;

  pthread_mutex_lock(r->ack);

  memset(tmp_cliaddr, 0, sizeof(tmp_cliaddr));

  // struct timespec {
  //     time_t tv_sec;
  //     ...    tv_nsec;
  // }
  iter_wait = {0, 999999999/(r->hz ? r->hz > 0 : LIMEADE_RECV_DEFAULT_HZ)};

  buffer = malloc(LIMEADE_RECV_TMP_SZ);
  if(!buffer)
    return NULL;

  while(pthread_mutex_trylock(r->mtx_kys) == EBUSY)
  {
    amt_recv = limeade_eth_recv(ctx, buffer, LIMEADE_RECV_TMP_SZ, &tmp_cliaddr,
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
  r   = ctx->recv;

  pthread_mutex_lock(r->ack);

  iter_wait = {0, 999999999/(r->hz ? r->hz > 0 : LIMEADE_RECV_DEFAULT_HZ)};

  buffer = malloc(LIMEADE_RECV_TMP_SZ);
  if(!buffer)
    return NULL;

  while(pthread_mutex_trylock(r->mtx_kys) == EBUSY)
  {
    amt_recv = limeade_eth_recv(ctx, buffer, LIMEADE_RECV_DEFAULT_SZ, NULL, 0);

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
  struct timespec iter_wait, rmtp;
  void *buffer;
  unsigned int amt_recv;

  ctx = arg;
  r = ctx->recv;
  
  pthread_mutex_lock(r->ack);

  iter_wait = {0, 999999999/(r->hz ? r->hz > 0 : LIMEADE_RECV_DEFAULT_HZ)};

  buffer = malloc(LIMEADE_RECV_TMP_SZ);
  if(!buffer)
    return NULL;

  while(pthread_mutex_trylock(r->mtx_kys) == EBUSY)
  {
    amt_recv = limeade_client_ssh_recv(ctx, buffer, LIMEADE_RECV_TMP_SZ);

    if(amt_recv == 0)
    {
      nanosleep(&iter_wait, &rmpt);
      continue;
    }
    if(amt_recv < 0)
      break;

    if(limeade_prelim_confimr(buffer, amt_recv) != 0)
      continue;

    limeade_th_recv_wr_pkt(r, buffer + MAGSZ, amt_recv, NULL, 0);

  }
  pthread_mutex_unlock(r->ack);
  free(buffer);
  return NULL;
}

#define limeade_th_recv_host_ssh limeade_th_recv_client_ssh

