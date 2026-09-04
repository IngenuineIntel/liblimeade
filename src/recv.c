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
    
    struct limeade_indiv_recv *d = r->pkts[r->wr_idx];
    pthread_mutex_unlock(r->mtx_idx);

    pthread_mutex_lock(d->mtx);
    memcpy(d->data, pkt, sz);
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

void *limeade_th_recv_client_eth(void *arg)
{
  struct limeade_context *ctx;
  struct limeade_recv_data *r;
  struct limeade_indiv_recv *d;
  struct timespec recv_wait, iter_wait, rmtp;
  void *buffer, *next, *prev;
  int amt_recv, t_amt_recv, rfd, hit_end;

  ctx = arg;
  r = ctx->recv;

  pthread_mutex_lock(r->mtx_ack);

  // struct timespec {
  //   time_t    tv_sec;
  //   /* ... */ tv_nsec;
  // };
  recv_wait = {0, 1000}; // 1µs
  iter_wait = {0, 1000000000/r->hz}; // (r->hz)hz
  // rmtp is simply because calls to `nanosleep` require an additional struct
  // for capturing the remaining unslept time in the event that the sleep must
  // end prematurely

  buffer = malloc(LIMEADE_RECV_TMP_SZ);

  pthread_mutex_lock(ctx->mtx_rfd);
  rfd = ctx->rfd;

  while(pthread_mutex_trylock(r->mtx_kys) == EBUSY)
  {
    if(nanosleep(&iter_wait, &rmtp) == -1)
      if(nanosleep(&rmtp, &rmtp) == -1)
        break;
    // TODO reevaluate how errors are handled outside the main thread

    t_amt_recv = amt_recv = 0;

    do
    {
      amt_recv = recvfrom(ctx->rfd, buffer + t_amt_recv, MSG_DONTWAIT, ctx->saddr, ctx->saddr_len);
      if(amt_recv <= 0)
      {

        if(errno == EAGAIN || errno == EWOULDBLOCK || amt_recv != 0)
          break; // out of data

        pthread_mutex_unlock(ctx->mtx_rfd);
        pthread_mutex_unlock(ctx->mtx_mode_union);
        goto err;
      }

      t_amt_recv += amt_recv;
    }
    pthread_mutex_unlock(ctx->mtx_rfd);
    pthread_mutex_unlock(ctx->mtx_mode_union);

    if(pthread_mutex_trylock(r->mtx_kts) != EBUSY)
      break;

    if(!t_amt_recv)
      continue;

    hit_end = 0;

    prev = memmem(buffer, t_amt_recv, &LIMEADE_MAGIC, MAGSZ);

    if(!prev)
      continue;

    t_amt_recv -= (prev - buffer);

    do
    {
      next = memmem(prev + MAGSZ, t_amt_recv, &LIMEADE_MAGIC_, MAGSZ);

      if(!next)
      {
        next = buffer + t_amt_recv;
        hit_end = 1;
      }

      limeade_th_recv_wr_pkt(prev, next - prev);

      if(hit_end)
        break;

      t_amt_recv -= (next - prev);
      prev = next;
    }
  }

err:
  pthread_mutex_unlock(r->mtx_ack);

  free(buffer);
  return NULL;

}

void *limeade_th_recv_client_ssh(void *arg)
{
  struct limeade_context *ctx;
  struct limeade_recv_data *r;
  struct limeade_indiv_recv *d;
  struct timespec recv_wait, iter_wait, rmtp;
  void *buffer, *next, *prev;
  int amt_recv, t_amt_recv, rfd, hit_end;

  ctx = arg;
  r = ctx->recv;

  pthread_mutex_lock(r->mtx_ack);

  recv_wait = {0, 1000};
  iter_wait = {0, 1000000000/r->hz};

  buffer = malloc(LIMEADE_RECV_TMP_SZ);
  pthread_mutex_lock(ctx->mtx_rfd);
  rfd = ctx->rfd;

  while(pthread_mutex_trylock(r->mtx_kys) == EBUSY)
  {
    nanosleep(&iter_wait, &rmtp)
  }
  
}

