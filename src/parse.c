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

int limeade_send_acknowledge(struct limeade_context *ctx, struct limeade_recvd pkt)
{
  int e;
  struct limeade_ack out;
  struct timespec t_recv;
  struct limeade_packet_flags *f = (struct limeade_packet_flags*)pkt.flags;
  e = limeade_monotonic(&t_recv);

  if(e != LIMEADE_SUCCESS)
    return e;

  out.send_ts_s  = f->ts_s;
  out.send_ts_ms = f->ts_ms;
  out.recv_ts_s  = t_recv.tv_sec;
  out.recv_ts_ms = t_recv.tv_nsec / 1000000;

  return limeade_send(ctx, LIMEADE_ACKNOWLEDGE, out);
}

int limeade_recv_noreply(struct limeade_context *ctx, struct limeade_recvd out)
{
  struct limeade_indiv_recv *r;
  struct limeade_packet_flags *f;

  pthread_mutex_lock(ctx->recv->mtx_idx);
  r = ctx->recv->pkts[ctx->recv->read_idx];
  pthread_mutex_unlock(ctx->recv->mtx_idx);

  pthread_mutex_lock(r->mtx);

  if(r->has_been_read != 0)
  {
    pthread_mutex_unlock(r->mtx);
    return LIMEADE_ERROR_NO_DATA;
  }

  r->has_been_read = 1;

  out.pkt = malloc(r->sz);
  if(!out.pkt)
  {
    pthread_mutex_unlock(r->mtx);
    return LIMEADE_ERROR_MEMORY;
  }
  memcpy(out.pkt, r->data, r->sz);

  pthread_mutex_unlock(r->mtx);

  out.data = out.pkt + sizeof(*f);
  f = out.pkt;
  out.type  = f->type;
  out.compr = f->compr_lvl;

  return LIMEADE_SUCCESS;
}

int limeade_recv(struct limeade_context *ctx, struct limeade_recvd out)
{
  int e = limeade_recv_noreply(ctx, out);
  if(e != LIMEADE_SUCCESS)
    return e;

  return limeade_send_acknowledge(ctx, out);
}

struct limeade_recv_waiter_data
{
  struct timespec wait_t;
  pthread_mutex_t mtx;
  pthread_t tid;
}

void *limeade_recv_waiter(void *arg)
{
  struct limeade_recv_wait_data *d = arg;
  pthread_mutex_lock(&d->indicator_mtx);
  nanosleep(&d->wait_t, &d->wait_t);
  pthread_mutex_unlock(&d->indicator_mtx);
  return NULL;
}

int limeade_recv_wait_noreply(struct limeade_context *ctx, struct limeade_recvd out, int wait_ms)
{
  int e;
  struct limeade_recv_waiter_data d;
  struct timespec wait_inc, rem;
  d.wait_t.tv_sec  = wait_ms / 1000;
  d.wait_t.tv_nsec = wait_ms * 1000000;
  wait_inc.tv_sec  = 0;
  wait_inc.tv_nsec = 100000000; // 10hz

  pthread_mutex_init(&d.mtx);
  pthread_create(&d.tid, NULL, limeade_recv_waiter, &d);

  do
  {
    nanosleep(&wait_inc, &rem);
    e = limeade_recv_noreply(ctx, out);

    if(e != LIMEADE_ERROR_NO_DATA)
      goto premature;

  } while(pthread_mutex_trylock(&d.mtx) == EBUSY);
  return LIMEADE_ERROR_NO_DATA;

premature:
  pthread_cancel(d.tid);
  return e;
}

int limeade_recv_wait(struct limeade_context *ctx, struct limeade_recvd out, int wait_ms)
{
  int e = limeade_recv_wait_noreply(ctx, out, wait_ms);
  if(e == LIMEADE_SUCCESS)
    return limeade_send_acknowledge(out);
  return e;
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

int limeade_parse_knock(struct limeade_knock *out, struct limeade_recvd pkt)
{
  // TODO
}

int limeade_parse_recognize(struct limeade_recognize *out, struct limeade_recvd pkt)
{
  // TODO
}

int limeade_parse_intro(struct limeade_intro *out, struct limeade_recvd pkt)
{
  // TODO
}

int limeade_parse_ack(struct limeade_ack *out, struct limeade_recvd pkt)
{
  // TODO
}

int limeade_parse_events(struct limeade_events *out, struct limeade_recvd pkt)
{
  // TODO
}

int limeade_parse_proc_generic(struct limeade_proc_generic *out, struct limeade_recvd pkt)
{
  // TODO
}

int limeade_parse_proc_update(struct limeade_proc_update *out, struct limeade_recvd pkt)
{
  // TODO
}

int limeade_parse_perf(struct limeade_perf *out, struct limeade_recvd pkt)
{
  // TODO
}

int limeade_parse_commandeer(struct limeade_commandeer *out, struct limeade_recvd pkt)
{
  // TODO
}

int limeade_parse_exited(struct limeade_exited *out, struct limeade_recvd pkt)
{
  // TODO
}

int limeade_parse_close(struct limeade_close *out, struct limeade_recvd pkt)
{
  // TODO
}


















