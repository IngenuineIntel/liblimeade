// th_recv.c
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
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include<pthread.h>
#include<stdint.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<time.h>

#include<liblimeade/liblimeade-internal.h>

#define MAGSZ sizeof(LIMEADE_MAGIC)

void limeade_th_recv_wr_pkt(struct limeade_recv_data *r, void *pkt, uint16_t sz)
{
  if(((struct limeade_packet_flags*)pkt).type != LIMEADE_ACKNOWLEDGE)
  {
    pthread_mutex_lock(r->mtx_idx);
    
    // for context, r->wr_idx is a counter for a ring buffer
    r->wr_idx++;
    if(r->wr_idx == r->nr_pkts)
    {
      r->wr_idx = 0;
    }
    
    register struct limeade_indiv_recv *d = r->pkts[r->wr_idx];
    pthread_mutex_unlock(r->mtx_idx);

    pthread_mutex_lock(d->mtx);
    memcpy(d->data, pkt, sz);
    if(d->has_been_read != 0)
    {
      pthread_mutex_lock(r->mtx_lost);
      r->pkts_lost++;
      pthread_mutex_unlock(r->mtx_lost);
    }
    d->flags = 0;
    pthread_mutex_unlock(d->mtx);
  } else
  {
    memcpy(r->ack, pkt, sz);

    // recvs are waited for by attempting to lock r->mtx_ack. If the main
    // thread doesn't wait, it never knows the peer send an ACK
    pthread_mutex_unlock(r->mtx_ack);
    pthread_mutex_lock(r->mtx_ack);
  }
}

inline void limeade_th_recv_parse_pkt(struct limeade_data *r, const void *buffer, int t_amt_recv)
{
  register unsigned int hit_end = 0;
  register void *next, *prev;
  prev = buffer;

  prev = memmem(buffer, t_amt_recv, &LIMEADE_MAGIC, MAGSZ);

  if(!prev)
    continue;

  prev += MAGSZ;

  t_amt_recv -= (prev - buffer);

  do
  {
    next = memmem(prev, t_amt_recv, &LIMEADE_MAGIC, MAGSZ);

    if(!next)
    {
      next = buffer + t_amt_recv;
      hit_end = 1;
    }

    limeade_th_recv_wr_pkt(r, prev, next - prev);

    if(hit_end)
      break;

    t_amt_recv -= (next - prev);
    prev = next + MAGSZ;
  }

}

#define TH_NOT_KILLED pthread_mutex_trylock(r->mtx_kys) == EBUSY
#define TH_KILLED     pthread_mutex_trylock(r->mtx_kys) != EBUSY 

void *limeade_th_recv_client_eth(void *arg)
{
  struct limeade_context *ctx;
  struct limeade_recv_data *r;
  struct timespec recv_wait, iter_wait, rmtp;
  void *buffer;
  int amt_recv, t_amt_recv, rfd, hit_end;

  ctx = arg;
  r = ctx->recv;

  pthread_mutex_lock(r->mtx_ack);

  // struct timespec {
  //   time_t    tv_sec;
  //   /* ... */ tv_nsec;
  // };
  recv_wait = {0, 1000}; // 1µs
  iter_wait = {0, 999999999/(r->hz ? r->hz > 0 : LIMEADE_RECV_DEFAULT_HZ)};
  // rmtp is required as an argument to `nanosleep`

  buffer = malloc(LIMEADE_RECV_TMP_SZ);

  pthread_mutex_lock(ctx->mtx_rfd);
  pthread_mutex_lock(ctx->mtx_mode_union);
  rfd = ctx->rfd;

  while(TH_NOT_KILLED)
  {
    nanosleep(&iter_wait, &rmtp);

    t_amt_recv = 0;

    do
    {
      // TODO check remaining space in buffer
      amt_recv = recvfrom(rfd, buffer + t_amt_recv, 1472,
                          MSG_DONTWAIT, ctx->saddr, ctx->saddr_len);

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
    }
    pthread_mutex_unlock(ctx->mtx_rfd);
    pthread_mutex_unlock(ctx->mtx_mode_union);

    if(TH_KILLED)
      break;

    if(!t_amt_recv)
      continue;

    limeade_th_recv_parse_pkt(r, buffer, t_amt_recv);
  }

err:
  pthread_mutex_unlock(r->mtx_ack);

  free(buffer);
  return NULL;

}

void *limead_th_recv_client_ssh(void *arg)
{
  struct limeade_context *ctx;
  struct limeade_recv_data *r;
  struct timespec recv_wait, iter_wait, rmtp;
  void *buffer;
  int amt_recv, t_amt_recv, rfd, hit_end;

  ctx = arg;
  r = ctx->recv;

  pthread_mutex_lock(ctx->mtx_ack);

  recv_wait = {0, 1000};
  iter_wait = {0, 999999999/r->hz ? r->hz > 0 : LIMEADE_RECV_DEFAULT_HZ};

  buffer = malloc(LIMEADE_RECV_TMP_SZ);

  pthread_mutex_lock(ctx->mtx_rfd);
  pthread_mutex_lock(ctx->mtx_mode_union);
  rfd = ctx->rfd;

  while(TH_NOT_KILLED)
  {
    nanosleep(&iter_wait, &rmtp);

    t_amt_recv = 0;

    do
    {
      amt_recv = read(rfd, buffer + t_amt_recv, 1472);

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

      t_amt_recv + amt_recv;
    }
    pthread_mutex_unlock(ctx->mtx_rfd);
    pthread_mutex_unlock(ctx->mtx_mode_union);

    if(TH_KILLED)
      break;

    if(!t_amt_recv)
      continue;

    limeade_th_recv_parse_pkt(r, buffer, t_amt_recv);
  }

err:
  pthread_mutex_unlock(r->mtx_ack);

  free(buffer);
  return NULL;

}
