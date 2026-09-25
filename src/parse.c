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

#include<errno.h> // IWYU pragma: keep
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

  return limeade_send(ctx, LIMEADE_PACKET_ACKNOWLEDGE, out);
}

int limeade_recv_noreply(struct limeade_context *ctx, struct limeade_recvd out)
{
  struct limeade_indiv_recv *r;
  struct limeade_packet_flags *f;

  pthread_mutex_lock(ctx->recv.mtx_idx);
  r = &ctx->recv.pkts[ctx->recv.read_idx];
  pthread_mutex_unlock(ctx->recv.mtx_idx);

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
};

void *limeade_recv_waiter(void *arg)
{
  struct limeade_recv_waiter_data *d = arg;
  pthread_mutex_lock(&d->mtx);
  nanosleep(&d->wait_t, &d->wait_t);
  pthread_mutex_unlock(&d->mtx);
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

  pthread_mutex_init(&d.mtx, NULL);
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
    return limeade_send_acknowledge(ctx, out);
  return e;
}
struct limeade_packet_flags limeade_parse_flags(struct limeade_recvd data)
{
  struct limeade_packet_flags ret;
  memcpy(&ret, data.flags, sizeof(ret));
  return ret;
}

static int limeade_cast_str(void *src, char **dst, int rem)
{
  register uint32_t l = limeade_pkt_strlen(src);
  *dst = malloc(l + 1);
  if(!*dst) return -1;
  memcpy(*dst, src, l);
  *(*dst + l) = '\x00';
  return l + 1;
}

#define ENFORCE_PKT_TYPE(pkt, TYPE)\
  if(((struct limeade_packet_flags*)pkt.flags)->type != TYPE)\
    return LIMEADE_ERROR_GARBAGE;

LIMEADE_CAST_FUNC(limeade_cast_u8, uint8_t, LIMEADE_TYPECHECK_UINT);
LIMEADE_CAST_FUNC(limeade_cast_u16, uint16_t, LIMEADE_TYPECHECK_UINT);
LIMEADE_CAST_FUNC(limeade_cast_u32, uint32_t, LIMEADE_TYPECHECK_UINT);
LIMEADE_CAST_FUNC(limeade_cast_u64, uint64_t, LIMEADE_TYPECHECK_UINT);
LIMEADE_CAST_FUNC(limeade_cast_i8, int8_t, LIMEADE_TYPECHECK_INT);
LIMEADE_CAST_FUNC(limeade_cast_i16, int16_t, LIMEADE_TYPECHECK_INT);
LIMEADE_CAST_FUNC(limeade_cast_i32, int32_t, LIMEADE_TYPECHECK_INT);
LIMEADE_CAST_FUNC(limeade_cast_i64, int64_t, LIMEADE_TYPECHECK_INT);
LIMEADE_CAST_FUNC(limeade_cast_flt, float, LIMEADE_TYPECHECK_FLT);
LIMEADE_CAST_FUNC(limeade_cast_dbl, double, LIMEADE_TYPECHECK_FLT);
#define _limeade_cast(dst, src, rem) _Generic((dst), \
  uint8_t*:  limeade_cast_u8,  \
  uint16_t*: limeade_cast_u16, \
  uint32_t*: limeade_cast_u32, \
  uint64_t*: limeade_cast_u64, \
  int8_t*:   limeade_cast_i8,  \
  int16_t*:  limeade_cast_i16, \
  int32_t*:  limeade_cast_i32, \
  int64_t*:  limeade_cast_i64, \
  float*:    limeade_cast_flt, \
  double*:   limeade_cast_dbl, \
  char**:    limeade_cast_str  \
)(dst, src, rem)

#define CAST_INIT()                                         \
register int _;                                             \
void *idx = pkt.data;                                       \
int rem = pkt.pkt_sz - sizeof(struct limeade_packet_flags); \
__attribute__((unused)) char last_delim;

#define CAST(dst)\
_ = _limeade_cast(dst, idx, rem); \
rem -= _;                         \
if(rem <= 0 || _ < 0) goto err;   \
idx += _;                         \
last_delim = *(char*)(idx - 1);

#define IF_NOT_ROW_END()\
if(last_delim != LIMEADE_ROW_DELIM)

int limeade_parse_knock(struct limeade_knock *out, struct limeade_recvd pkt)
{
  ENFORCE_PKT_TYPE(pkt, LIMEADE_PACKET_KNOCK);

  // these packet should never really be compressed anyway, but...
  if(limeade_decompress_packet(pkt))
    return LIMEADE_ERROR_COMPRESSION;

  CAST_INIT();
  CAST(&out->prev_connected);
  CAST(&out->prev_session);

  return LIMEADE_SUCCESS;

err:
  return LIMEADE_ERROR_BAD_DATA;
}

int limeade_parse_recognize(struct limeade_recognize *out, struct limeade_recvd pkt)
{

  ENFORCE_PKT_TYPE(pkt, LIMEADE_PACKET_RECOGNIZE);

  if(limeade_decompress_packet(pkt))
    return LIMEADE_ERROR_COMPRESSION;

  if(limeade_get_nr_rows(&pkt) != 1)
    return LIMEADE_ERROR_BAD_DATA;

  CAST_INIT();
  CAST(&out->accepted);
  CAST(&out->new_session);

  return LIMEADE_SUCCESS;

err:
  return LIMEADE_ERROR_BAD_DATA;
}

int limeade_parse_intro(struct limeade_intro *out, struct limeade_recvd pkt)
{
  ENFORCE_PKT_TYPE(pkt, LIMEADE_PACKET_INTRO);

  if(limeade_decompress_packet(pkt))
    return LIMEADE_ERROR_COMPRESSION;

  if(limeade_get_nr_rows(&pkt) != 1)
    return LIMEADE_ERROR_BAD_DATA;

  CAST_INIT();
  CAST(&out->hostname);
  CAST(&out->kernelver);
  CAST(&out->distro);
  CAST(&out->origin_user);
  CAST(&out->processor);
  CAST(&out->vendor);
  CAST(&out->ram_mbs);
  CAST(&out->swap_mbs);

  return LIMEADE_SUCCESS;

err:
  return LIMEADE_ERROR_BAD_DATA;
}

int limeade_parse_ack(struct limeade_ack *out, struct limeade_recvd pkt)
{
  ENFORCE_PKT_TYPE(pkt, LIMEADE_PACKET_ACK);

  if(limeade_decompress_packet(pkt))
    return LIMEADE_ERROR_COMPRESSION;

  if(limeade_get_nr_rows(&pkt) != 1)
    return LIMEADE_ERROR_BAD_DATA;

  CAST_INIT();
  CAST(&out->send_ts_s);
  CAST(&out->send_ts_ms);
  CAST(&out->recv_ts_s);
  CAST(&out->recv_ts_ms);
  
  return LIMEADE_SUCCESS;

err:
  return LIMEADE_ERROR_BAD_DATA;
}

int limeade_parse_events(struct limeade_events *out, struct limeade_recvd pkt)
{
  void *alloc;
  struct limeade_indiv_event *j;

  ENFORCE_PKT_TYPE(pkt, LIMEADE_PACKET_EVENTS);

  if(limeade_decompress_packet(pkt))
    return LIMEADE_ERROR_COMPRESSION;

  CAST_INIT();
  CAST(&out->nr_events);
  IF_NOT_ROW_END()
    return LIMEADE_ERROR_BAD_DATA;

  if(limeade_get_nr_rows(&pkt) != out->nr_events + 1)
    return LIMEADE_ERROR_BAD_DATA;

  alloc = malloc(sizeof(*j) * out->nr_events);
  if(!alloc)
    return LIMEADE_ERROR_MEMORY;

  for(int i = 0; i < out->nr_events; i++)
  {
    j = &out->events[i];
    CAST(&j->ts_s);
    CAST(&j->ts_ms);
    CAST(&j->pid);
    CAST(&j->syscall);
    CAST(&j->arg1);
    CAST(&j->arg2);
    CAST(&j->retval);
    IF_NOT_ROW_END()
      goto err;
  }

  return LIMEADE_SUCCESS;

err:
  for(int i = 0; i < out->nr_events && &out->events[i]; i++)
  {
    j = &out->events[i];
    free(j->syscall);
    free(j->arg1);
    free(j->arg2);
  }
  free(alloc);

  return LIMEADE_ERROR_BAD_DATA;
}

int limeade_parse_proc_generic(struct limeade_proc_generic *out, struct limeade_recvd pkt)
{
  void *alloc;
  struct limeade_indiv_proc *j;

  ENFORCE_PKT_TYPE(pkt, LIMEADE_PACKET_PROC_GENERIC);

  if(limeade_decompress_packet(pkt))
    return LIMEADE_ERROR_COMPRESSION;

  CAST_INIT();
  CAST(&out->total);

  IF_NOT_ROW_END()
    return LIMEADE_ERROR_BAD_DATA;

  if(limeade_get_nr_rows(&pkt) != out->total + 1)
    return LIMEADE_ERROR_BAD_DATA;

  alloc = malloc(sizeof(*j) * out->total);
  if(!alloc)
    return LIMEADE_ERROR_MEMORY;
  
  for(int i = 0; i < out->total; i++)
  {
    j = &out->procs[i];
    CAST(&j->pid);
    CAST(&j->ppid);
    CAST(&j->uid);
    CAST(&j->threads);
    CAST(&j->cpu_ticks);
    CAST(&j->vm_rss_kb);
    CAST(&j->command);
    IF_NOT_ROW_END()
      goto err;
  }

  return LIMEADE_SUCCESS;

err:
  for(int i = 0; i < out->total && &out->procs[i]; i++)
    free(out->procs[i].command);
  free(alloc);

  return LIMEADE_ERROR_BAD_DATA;
}

int limeade_parse_proc_update(struct limeade_proc_update *out, struct limeade_recvd pkt)
{
  void *alloc;
  int t_sz;
  struct limeade_indiv_proc *j;

  ENFORCE_PKT_TYPE(pkt, LIMEADE_PACKET_PROC_UPDATE);

  if(limeade_decompress_packet(pkt))
    return LIMEADE_ERROR_COMPRESSION;
  
  CAST_INIT();

  CAST(&out->total_died);
  CAST(&out->total_altered);

  IF_NOT_ROW_END()
    return LIMEADE_ERROR_BAD_DATA;

  if(limeade_get_nr_rows(&pkt) != out->total_altered + 2)
    return LIMEADE_ERROR_BAD_DATA;

  t_sz = out->total_died    * sizeof(pid_t)
       + out->total_altered * sizeof(*out->altered);

  alloc = malloc(t_sz);

  if(!alloc)
    return LIMEADE_ERROR_MEMORY;

  out->died    = alloc;
  out->altered = alloc + out->total_died * sizeof(pid_t);

  for(int i = 0; i < out->total_died; i++)
    CAST(&out->died[i]);

  IF_NOT_ROW_END()
    goto err;

  for(int i = 0; i < out->total_altered; i++)
  {
    j = &out->altered[i];
    CAST(&j->pid);
    CAST(&j->ppid);
    CAST(&j->uid);
    CAST(&j->threads);
    CAST(&j->cpu_ticks);
    CAST(&j->vm_rss_kb);
    CAST(&j->command);
    IF_NOT_ROW_END()
      goto err;
  }

  return LIMEADE_SUCCESS;

err:
  for(int i = 0; i < out->total_altered && &out->altered[i]; i++)
    free(out->altered[i].command);
  free(alloc);

  return LIMEADE_ERROR_BAD_DATA;
}

int limeade_parse_perf(struct limeade_perf *out, struct limeade_recvd pkt)
{
  ENFORCE_PKT_TYPE(pkt, LIMEADE_PACKET_PERF);

  if(limeade_decompress_packet(pkt))
    return LIMEADE_ERROR_COMPRESSION;

  if(limeade_get_nr_rows(&pkt) != 1)
    return LIMEADE_ERROR_BAD_DATA;

  CAST_INIT();
  CAST(&out->cores);
  CAST(&out->avg_cpu_pct);
  CAST(&out->mem_total_kb);
  CAST(&out->mem_free_kb);
  CAST(&out->mem_available_kb);
  CAST(&out->mem_cached_kb);
  CAST(&out->load_1m);
  CAST(&out->load_5m);
  CAST(&out->load_15m);
  CAST(&out->other);

  return LIMEADE_SUCCESS;

err:
  return LIMEADE_ERROR_BAD_DATA;
}

int limeade_parse_commandeer(struct limeade_commandeer *out, struct limeade_recvd pkt)
{
  ENFORCE_PKT_TYPE(pkt, LIMEADE_PACKET_COMMANDEER);

  if(limeade_decompress_packet(pkt))
    return LIMEADE_ERROR_COMPRESSION;

  if(limeade_get_nr_rows(&pkt) != 1)
    return LIMEADE_ERROR_BAD_DATA;

  CAST_INIT();
  CAST(&out->command);
  CAST(&out->flags);
  CAST(&out->id);

  return LIMEADE_SUCCESS;

err:
  return LIMEADE_ERROR_BAD_DATA;
}

int limeade_parse_exited(struct limeade_exited *out, struct limeade_recvd pkt)
{
  ENFORCE_PKT_TYPE(pkt, LIMEADE_PACKET_EXITED);

  if(limeade_decompress_packet(pkt))
    return LIMEADE_ERROR_COMPRESSION;

  if(limeade_get_nr_rows(&pkt) != 1)
    return LIMEADE_ERROR_BAD_DATA;

  CAST_INIT();
  CAST(&out->id);
  CAST(&out->exitcode);

  return LIMEADE_SUCCESS;

err:
  return LIMEADE_ERROR_BAD_DATA;
}

int limeade_parse_close(struct limeade_close *out, struct limeade_recvd pkt)
{
  ENFORCE_PKT_TYPE(pkt, LIMEADE_PACKET_CLOSE);

  if(limeade_decompress_packet(pkt))
    return LIMEADE_ERROR_COMPRESSION;

  if(limeade_get_nr_rows(&pkt) != 1)
    return LIMEADE_ERROR_BAD_DATA;

  CAST_INIT();
  CAST(&out->explanation);

  return LIMEADE_SUCCESS;

err:
  return LIMEADE_ERROR_BAD_DATA;
}

#undef ENFORCE_PKT_TYPE
#undef CAST_INIT
#undef CAST
#undef IF_NOT_ROW_END
#undef _limeade_cast

