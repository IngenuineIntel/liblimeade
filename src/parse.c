// parse.c
// receiving and parsing
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
#include<stdlib.h>
#include<string.h>

#include<liblimeade/liblimeade-internal.h>

void limeade_send_acknowledge(struct limeade_context *ctx, struct limeade_recvd pkt)
{
  struct limeade_ack out;
  struct timespec t_recv;
  struct limeade_packet_flags *f = (struct limeade_packet_flags*)pkt.flags;
  limeade_monotonic(&t_recv);
  LIMEADE_CHECK();

  out.send_ts_s  = f->ts_s;
  out.send_ts_ms = f->ts_ms;
  out.recv_ts_s  = t_recv.tv_sec;
  out.recv_ts_ms = t_recv.tv_nsec / 1000000;

  limeade_send(ctx, LIMEADE_ACKNOWLEDGE, out);
  LIMEADE_CHECK();

  limeade_pusherr(LIMEADE_SUCCESS);
  err:
  return;
}

struct limeade_recvd limeade_recv_noreply(struct limeade_context *ctx)
{
  struct limeade_recvd ret;
  struct limeade_indiv_recv *r;
  struct limeade_packet_flags *f;

  pthread_mutex_lock(ctx->recv->mtx_idx);

  r = ctx->recv->pkts[ctx->recv->read_idx];

  pthread_mutex_unlock(ctx->recv->mtx_idx);

  pthread_mutex_lock(r->mtx);

  ret.pkt = malloc(r->sz);
  memcpy(ret.pkt, r->data, r->sz);

  LIMEADE_CHECKEXPR(r->has_been_read != 0, LIMEADE_ERROR_NO_DATA);

  r->has_been_read = 1;

  ret.pkt_sz = r->sz;

  pthread_mutex_unlock(r->mtx);

  ret.data = ret.pkt + sizeof(struct limeade_packet_flags);
  
  f = (struct limeade_packet_flags*)ret.pkt;
  ret.type = f->type;
  ret.compr = f->compr_lvl;

  limeade_pusherr(LIMEADE_SUCCESS);

  err:
  return ret;
}

struct limeade_recvd limeade_recv(struct limeade_context *ctx)
{
  struct limeade_recvd ret = limeade_recv_noreply(ctx);
  LIMEADE_CHECK();

  limeade_send_acknowledge(ctx, ret);
  LIMEADE_CHECK();

  limeade_inserr(LIMEADE_SUCCESS);

  err:
  
  return ret;
}

struct limeade_recv_waiter_data
{
  struct timespec wait_t;
  pthread_mutex_t mtx;
  pthread_t tid;
}

void *limeade_recv_waiter(void *arg)
{
  struct limeade_recv_wait_data *d = (struct limeade_recv_waiter_data*)arg;
  pthread_mutex_lock(&d->indicator_mtx);
  nanosleep(&d->wait_t, &d->wait_t);
  pthread_mutex_unlock(&d->indicator_mtx);
  return NULL;
}

struct limeade_recvd limeade_recv_wait_noreply(struct limeade_context *ctx, int wait_ms)
{
  struct limeade_recv_waiter_data d;
  d.wait_t.tv_sec  = wait_ms / 1000;
  d.wait_t.tv_nsec = wait_ms * 1000000;

  pthread_mutex_init(&d.mtx);
  pthread_create(&d.tid, NULL, limeade_recv_waiter, &d);

  do
  {
    // TODO
    // TODO try recv (return straight out of the loop
  } while(pthread_mutex_trylock(&d.mtx) == EBUSY);
  limeade_inserr(LIMEADE_ERROR_NO_DATA);
  struct limeade_recv r;
  return r;
}

struct limeade_recvd limeade_recv_wait(struct limeade_context *ctx, int wait_ms)
{
  struct limeade_recvd ret = limeade_recv_wait_noreply(ctx, wait_ms);
  LIMEADE_CHECK();

  limeade_send_acknowledge(ctx, ret);
  LIMEADE_CHECK();

  limeade_inserr(LIMEADE_SUCCESS);

err:
  return ret;
}

struct limeade_packet_flags limeade_parse_flags(struct limeade_recv data)
{
  struct limeade_packet_flags ret;
  int len = memcpy(&ret, data.flags, sizeof(limeade_packet_flags));
  if(len != sizeof(limeade_packet_flags))
  {
    limeade_inserr(LIMEADE_ERROR_BAD_DATA);
  } else
  {
    limeade_inserr(LIMEADE_SUCCESS);
  }
  return ret;
}

struct limeade_knock limeade_parse_knock(struct limeade_recv pkt)
{
  if(pkt.type != LIMEADE_PACKET_KNOCK)
  {
    limeade_inserr(LIMEADE_ERROR_GARBAGE);
    return (struct limeade_knock)NULL;
  }
}

struct limeade_recognize limeade_parse_recognize(struct limeade_recv pkt)
{
  if(pkt.type != LIMEADE_PACKET_RECOGNIZE)
  {
    return (struct limeade_recognize)NULL;
  }
}
struct limeade_intro limeade_parse_intro(struct limeade_recv pkt)
{
  if(pkt.type != LIMEADE_PACKET_INTRO)
  {
    return (struct limeade_intro)NULL;
  }
}
struct limeade_ack limeade_parse_ack(struct limeade_recv pkt)
{
  if(pkt.type != LIMEADE_PACKET_ACK)
  {
    limeade_inserr(LIMEADE_ERROR_GARBAGE);
    return (struct limeade_knock)NULL;
  }
}
struct limeae_events limeade_parse_events(struct limeade_recv pkt)
{
  if(pkt.type != LIMEADE_PACKET_EVENTS)
  {
    limeade_inserr(LIMEADE_ERROR_GARBAGE);
    return (struct limeade_knock)NULL;
  }
}
struct limeade_proc_generic limeade_parse_proc_generic(struct limeade_recv pkt)
{
  if(pkt.type != LIMEADE_PACKET_PROC_GENERIC)
  {
    limeade_inserr(LIMEADE_ERROR_GARBAGE);
    return (struct limeade_knock)NULL;
  }
}
struct limeade_proc_update limeade_parse_proc_update(struct limeade_recv pkt)
{
  if(pkt.type != LIMEADE_PACKET_PROC_UPDATE)
  {
    limeade_inserr(LIMEADE_ERROR_GARBAGE);
    return (struct limeade_knock)NULL;
  }
}
struct limeade_perf limeade_parse_perf(struct limeade_recv pkt)
{
  if(pkt.type != LIMEADE_PACKET_PERF)
  {
    limeade_inserr(LIMEADE_ERROR_GARBAGE);
    return (struct limeade_knock)NULL;
  }
}
struct limeade_commandeer limeade_parse_commandeer(struct limeade_recv pkt)
{
  if(pkt.type != LIMEADE_PACKET_COMMANDEER)
  {
    limeade_inserr(LIMEADE_ERROR_GARBAGE);
    return (struct limeade_knock)NULL;
  }
}
struct limeade_exited limeade_parse_exited(struct limeade_recv pkt)
{
  if(pkt.type != LIMEADE_PACKET_EXITED)
  {
    limeade_inserr(LIMEADE_ERROR_GARBAGE);
    return (struct limeade_knock)NULL;
  }
}
struct limeade_close limeade_parse_close(struct limeade_recv pkt)
{
  if(pkt.type != LIMEADE_PACKET_CLOSE)
  {
    limeade_inserr(LIMEADE_ERROR_GARBAGE);
    return (struct limeade_knock)NULL;
  }
}
