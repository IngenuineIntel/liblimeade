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
  /* Writes packet information into the process-wide cache */
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

inline void limeade_th_recv_parse_pkt(struct limeade_recv_data *r,
                                      const void *buffer, int t_amt_recv)
{
  unsigned int hit_end = 0;
  void *next, *prev;
  struct limeade_packet_flags *flags;
  prev = buffer;

  do
  {
    next = memmem(prev, t_amt_recv, &LIMEADE_MAGIC, MAGSZ);

    if(!next)
      return;

    next += MAGSZ;

    // note: my understanding of this (I haven't checked the assembly yet) is that
    // the pointer subtraction is done in a 64-bit register and likewise doesn't
    // create an unexpected number in t_amt_recv.
    t_amt_recv -= (next - buffer);

    flags = (struct limeade_packet_flags*)next;

    if(flags.pkt_sz > t_amt_recv || flags.type >= LIMEADE_PACKET_MAX)
      return;

    limeade_th_recv_wr_pkt(r, next, flags.pkt_sz);

    prev = next + flags.pkt_sz;
    t_amt_recv -= flags.pkt_sz;
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

void *limeade_th_recv_host_eth(void *arg)
{
  struct limeade_context *ctx;
  struct limeade_recv_data *r;
  struct limeade_eth_client *c;
  struct limeade_packet_flags *f;
  struct timespec recv_wait, iter_wait, rmtp;
  struct sockaddr_in tmp_cliaddr;
  socklen_t tmp_len;
  void *buffer, *next;
  unsigned int amt_recv;

  ctx = arg;
  ctx->clients = malloc(sizeof(*c));
  c = ctx->clients;
  r = ctx->recv;
  if(!c)
    return NULL;
  memset(c, 0, sizeof(*c));
  

  pthread_mutex_lock(r->mtx_ack);

  recv_wait = {0, 500}; // 0.5µs
  iter_wait = {0, 999999999/(r->hz ? r->hz > 0 : LIMEADE_RECV_DEFAULT_HZ)};

  buffer = malloc(LIMEADE_RECV_TMP_SZ);
  if(!buffer)
  {
    free(ctx->clients);
    return NULL;
  }

  while(TH_NOT_KILLED)
  {

    pthread_mutex_lock(ctx->mtx_rfd);
    pthread_mutex_lock(ctx->mtx_mode_union);

    amt_recv = recvfrom(ctx->rfd, buffer, LIMEADE_RECV_TMP_SZ, 0,
                        (struct sockaddr*)&cliaddr, &client_len);

    pthread_mutex_unlock(ctx->mtx_rfd);
    pthread_mutex_unlock(ctx->mtx_mode_union);

    if(!amt_recv)
    {
      nanosleep(&iter_wait, &rmtp);
      continue;
    }
    

    next = memmem(buffer, amt_recv, &LIMEADE_MAGIC, MAGSZ);
    if(!next)
      continue;

    next += MAGSZ;
    if(next + sizeof(*f) > buffer + amt_recv)
      continue;

    f = next;
    if(f->type >= LIMEADE_PACKET_MAX)
      continue;

    if(f->pkt_sz >= (buffer + amt_recv - next))
      continue;

    if(f->session == 0)
    {
      if(f->type != LIMEADE_PACKET_KNOCK)
        continue;
      
      struct limeade_packet_data pkt;
      struct limeade_knock contents;

      pkt.pkt    = buffer;
      pkt.pkt_sz = f->pkt_sz;
      pkt.type   = f->type;
      pkt.data   = pkt.pkt + sizeof(*f);

      if(limeade_parse_knock(&contents, &pkt) != LIMEADE_SUCCESS)
        continue;

      // TODO limeade_client_check_session & limeade_client_register require
      // the client list to be iterated over twice, which is unecessary
      LIMEADE_SESSION pref_session =
        contents.prev_session ? contents.prev_session != 0
                             && limeade_client_check_session(c, contents.prev_session) == 0
                              : limeade_client_gen_session();

      limeade_client_register(c, cliaddr, cliaddr_len, pref_session);
    }

    if(limeade_confirm_preexisting(c, &cliaddr, session) != 0)
      continue;

    limeade_th_recv_wr_pkt(r, buffer, amt_recv);

  }
}

