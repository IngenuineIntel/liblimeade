// send.c
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

#include <stdarg.h>
#include <stdio.h>

#include <zlib.h>

#include <liblimeade/liblimeade-internal.h>

static void prep_str(char *in)
{
  for(char *i = in; (void*)i > NULL; i++)
  {
    if(*i == LIMEADE_FIELD_DELIM || *i == LIMEADE_ROW_DELIM)
    {
      *i = ' ';
    } else if (*i == '\x00')
    {
      break;
    }
  }
}

static int prep_uint(uint64_t in, char *out, unsigned int sz)
{
  int ret = snprintf(out, sz, "%luX", in);
  prep_str(out);
  return ret;
}

static prep_int(int64_t in, char *out, unsigned int sz)
{
  int ret = snrprintf(out, sz, "%lX", in);
  prep_str(out);
  return ret;
}

static prep_float(double in, char *out, unsigned int sz)
{
  int ret = snprintf(out, sz, "%f", in);
  prep_str(out);
  return ret;
}

#define CLS_INTREPR() memset(&int_repr, 0x00, sizeof(int_repr));
#define CLS_DBLREPR() memset(&dbl_repr, 0x00, sizeof(dbl_repr));
#define WRS(x)                     \
if(x != NULL)                      \
{                                  \
  len = strlen(x);                 \
  char tmp[len + 1];               \
  strcpy(&tmp, x);                 \
  prep_str(&tmp);                  \
  limeade_wr_at_idx(d, &tmp, len); \
}
#define WRU(x)                                     \
{                                                  \
  len = prep_uint(x, &int_repr, sizeof(int_repr)); \
  limeade_wr_at_idx(d, &int_repr, len);            \
  CLS_INTREPR();                                   \
}
#define WRI(x)                                    \
{                                                 \
  len = prep_int(x, &int_repr, sizeof(int_repr)); \
  limeade_wr_at_idx(d, &int_repr, len);           \
  CLS_INTREPR();                                  \
}
#define WRF(x)                                      \
{                                                   \
  len = prep_float(x, &dbl_repr, sizeof(dbl_repr)); \
  limeade_wr_at_idx(d, &dbl_repr, len);             \
  CLS_DBLREPR();                                    \
}
#define FDELIM() limeade_wr_at_idx(d, &LIMEADE_FIELD_DELIM, 1);
#define RDELIM() limeade_wr_at_idx(d, &LIMEADE_ROW_DELIM, 1);
// structure for passing to wr_at_idx
struct limeade_pkt_wr_data
{
  struct limeade_packet_data *pkt;
  unsigned int wr_idx;
  unsigned int realloc_inc;
};

static void limeade_wr_at_idx(struct limeade_pkt_wr_data d, void *data, int len)
{
  if(d.pkt->pkt_size < d.wr_idx + len)
  {
    d.pkt->pkt_sz += d.realloc_inc + len;
    d.pkt->pkt = realloc(d.pkt->pkt, d.pkt->pkt_size);
    // TODO manage error (maybe return -1?)
  }
  memcpy(d.pkt->pkt + d.wr_idx, data, len);
  d.wr_idx += len;
}

int limeade_populate_packet(struct limeade_packet_data *in,
                                    struct limeade_packet_flags *flags,
                                    va_list arg)
{
  struct limeade_pkt_wr_data d = {
    .pkt = in,
    .wr_idx = sizeof(struct limeade_packet_flags),
    .realloc_increment = 2048 // arbitrary value btw
  };

  in->pkt = malloc(d.realloc_increment * 2); // *2 is arbitrary coefficient

  memcpy(in->pkt, flags, sizeof(struct limeade_packet_flags));

  char int_repr[18]; // 18 == len(hex(-2 ** 64))-2+1 == len(hex(2**64))-2+1
  char dbl_repr[32]; // arbitrary value
  int len;

  switch(in->type)
  {
    case LIMEADE_PACKET_KNOCK:
    {
      struct limeade_knock data = va_arg(arg, struct limeade_knock);

      WRU(data.prev_connected);
      FDELIM();
      WRU(data.prev_session);
      RDELIM();
    }
    case LIMEADE_PACKET_RECOGNIZE:
    {
      struct limeade_recognize data = va_arg(arg, struct limeade_recognize);

      WRU(data.accepted);
      FDELIM();
      WRU(data.new_session);
      RDELIM();
    }
    case LIMEADE_PACKET_INTRODUCTION:
    {
      struct limeade_intro data = va_arg(arg, struct limeade_intro);

      WRS(data.hostname);
      FDELIM();
      WRS(data.kernelver);
      FDELIM();
      WRS(data.distro);
      FDELIM();
      WRS(data.origin_user);
      FDELIM();
      WRS(data.processor);
      FDELIM();
      WRS(data.vendor);
      FDELIM();
      WRU(data.ram_mbs);
      FDELIM();
      WRU(data.swap_mbs);
      RDELIM();
    }
    case LIMEADE_PACKET_ACKNOWLEDGE:
    {
      struct limeade_ack data = va_arg(arg, struct limeade_ack);

      WRU(data.send_ts_s);
      FDELIM();
      WRU(data.send_ts_ms);
      FDELIM();
      WRU(data.recv_ts_s);
      FDELIM();
      WRU(data.recv_ts_ms);
      RDELIM();
    }
    case LIMEADE_PACKET_EVENTS:
    {
      struct limeadde_events data = va_arg(arg, struct limeade_events);

      struct limeade_indiv_event *j;

      for(int i = 0; i < data.nr_events; i++)
      {
        j = data.events[i];

        WRU(j->ts_s);
        FDELIM();
        WRU(j->ts_ms);
        FDELIM();
        WRU(j->pid);
        FDELIM();
        WRS(j->syscall);
        FDELIM();
        WRS(j->arg1);
        FDELIM();
        WRS(j->arg2);
        FDELIM();
        WRI(j->retval);
        RDELIM();
      }
    }
    case LIMEADE_PACKET_PROC_GENERIC:
    {
      struct limeade_proc_generic data = va_arg(arg, struct limeade_proc_generic);

      struct limeade_indiv_proc *j;

      for(int i = 0; i < data.total; i++)
      {
        j = data.procs[i];

        WRU(j->pid);
        FDELIM();
        WRU(j->ppid);
        FDELIM();
        WRU(j->uid);
        FDELIM();
        WRU(j->threads);
        FDELIM();
        WRU(j->cpu_ticks);
        FDELIM();
        WRU(j->ram_kb);
        FDELIM();
        WRS(j->command);
        RDELIM();
      }
    }
    case LIMEADE_PACKET_PROC_UPDATE:
    {
      struct limeade_proc_update data = va_arg(arg, struct limeade_proc_update);

      struct limeade_indiv_proc *j;
      int i, died;

      WRU(data.total_died);
      FDELIM();
      WRU(data.total_altered);
      RDELIM();

      died = data.died - 1;
      for(i = 0; i < died; i++)
      {
        WRU(data.died[i]);
        FDELIM();
      }
      // write last in series outside to loop to avoid putting an FDELIM right
      // before an RDELIM
      WRU(data.died[died]);
      RDELIM();

      for(i = 0; < data.total_altered; i++)
      {
        j = data.altererd[i];

        WRU(j->pid);
        FDELIM();
        WRU(j->ppid);
        FDELIM();
        WRU(j->uid);
        FDELIM();
        WRU(j->threads);
        FDELIM();
        WRU(j->cpu_ticks);
        FDELIM();
        WRU(j->ram_kb);
        FDELIM();
        WRS(j->command);
        RDELIM();
      }
    }
    case LIMEADE_PACKET_PERF:
    {
      struct limeade_perf data = va_arg(arg, struct limeade_perf);

      WRU(data.cores);
      FDELIM();
      WRU(data.avg_cpu_pct);
      FDELIM();
      WRU(data.mem_total_kb);
      FDELIM();
      WRU(data.mem_free_kb);
      FDELIM();
      WRU(data.mem_available_kb);
      FDELIM();
      WRU(data.mem_cached_kb);
      FDELIM();
      WRF(data.load_1m);
      FDELIM();
      WRF(data.load_5m);
      FDELIM();
      WRF(data.load_15m);
      FDELIM();
      WRD(data.other);
      RDELIM();
    }
    case LIMEADE_PACKET_COMMANDEER:
    {
      struct limeade_commandeer data = va_arg(arg, struct limeade_commandeer);

      WRS(data.command);
      FDELIM();
      WRU(data.flags);
      FDELIM();
      WRU(data.id);
      RDELIM();
    }
    case LIMEADE_PACKET_EXITED:
    {
      struct limeade_exited data = va_arg(arg, struct limeade_exited);

      WRU(data.uint32_t);
      FDELIM();
      WRI(data.exitcode);
      RDELIM();
    }
    case LIMEADE_PACKET_CLOSE:
    {
      struct limeade_close data = va_arg(arg, struct limeade_close);

      WRS(data.explanation);
      RDELIM();
    }
  }

  return LIMEADE_SUCCESS;
}

int limeade_deflate_packet(struct limeade_packet_data *pkt, int compr_lvl)
{
  if(compr_lvl <= 0)
  {
    limeade_inserr(LIMEADE_SUCCESS);
    return;
  }

  void *compressed = malloc(pkt->pkt_sz);
  unsigned long compressed_len;

  if(compress2(compressed, &compressed_len, pkt->data,
               pkt->pkt_sz - sizeof(struct limeade_packet_flags),
               compr_lvl) != Z_OK)
  {
    free(compressed);
    return LIMEADE_ERROR_COMPRESSION;
  }

  if(memcpy(pkt->data, compressed, compressed_len) != compressed_len)
  {
    free(compressed);
    return LIMEADE_ERROR_COMPRESSION;
  }

  free(compressed);

  void *new_pkt = realloc(pkt->pkt, compressed_len + sizeof(struct limeade_packet_flags));

  if(new_pkt == NULL)
  {
    return LIMEADE_ERROR_MEMORY;
  }

  pkt->pkt = new_pkt;
  pkt->data = new_pkt + sizeof(struct limeade_packet_flags);
  pkt->pkt_sz = compressed_len + sizeof(struct limeade_packet_flags);

  return LIMEADE_SUCCESS;
}

int limeade_send_base(struct limeade_context *ctx, struct limeade_packet_data *pkt)
{
  if(ctx->mode == LIMEADE_MODE_HOST_SSH || ctx->mode == LIMEADE_MODE_CLIENT_SSH)
  {
    if(write(ctx->sfd, &LIMEADE_MAGIC, sizeof(LIMEADE_MAGIC)) != sizeof(LIMEADE_MAGIC))
    {
      return LIMEADE_ERROR_NETWORKING;
    } else if(write(ctx->sfd, pkt->pkt, pkt->pkt_sz) != pkt->pkt_sz)
    {
      return LIMEADE_ERROR_NETWORKING;
    } else
    {
      return LIMEADE_SUCCESS;
    }
  } else if(ctx->mode == LIMEADE_MODE_HOST_ETH
         || ctx->mode == LIMEADE_MODE_CLIENT_ETH)
  {
    // TODO
  } else if(ctx->mode == LIMEADE_MODE_CLIENT_LIBSSH)
  {
    // TODO
  } else
  {
    return LIMEADE_INVALID_CONTEXT;
  }

  return LIMEADE_SUCCESS;
}

int limeade_send(struct limeade_context *ctx, limeae_packet type, ...)
{
  va_list arg;
  va_start(arg, type);

  struct limeade_packet_data pkt;
  struct limeade_packet_flags flags;
  struct limeade_csm_compression_entry entry;
  struct timespec compr_ts[2], sendts;

  limeade_monotonic(&compr_ts[0]);
  LIMEADE_CHECK();

  entry.compr_lvl = ctx->compr_lvl;

  pkt.type = flags.type = type;

  pthread_mutex_lock(ctx->mtx_compr);
  flags.compr_lvl = ctx->compr_lvl;
  pthread_mutex_unlock(ctx->mtx_compr);

  limeade_populate_packet(&pkt, &flags, arg);
  LIMEADE_CHECK();

  entry.precompr_sz = pkt.pkt_sz;

  limeade_deflate_packet(&pkt, flags.compr_lvl);
  LIMEADE_CHECK();

  entry.postcompr_sz = pkt.pkt_sz;

  limeade_monotonic(&sendts);
  LIMEADE_CHECK();

  ((struct limeade_packet_flags*)pkt.flags)->packet_size = pkt->pkt_sz;
  ((struct limeade_packet_flags*)pkt.flags)->ts_s = sendts.tv_sec;
  ((struct limeade_packet_flags*)pkt.flags)->ts_ms = sendts.tv_nsec / 100;

  limeade_monotonic(&compr_ts[1]);
  LIMEADE_CHECK();

  limeade_base_send(ctx, &pkt);
  LIMEADE_CHECK();

  entry.elapsed_ms = limeade_monotonic_diff_ms(&compr_ts[0], &compr_ts[1]);

  limeade_csm_add_compr_entry(ctx, &entry);
  LIMEADE_CHECK();

  free(pkt.pkt);
  return LIMEADE_SUCCESS;

  err:
  
  free(pkt.pkt);
  return -1;
}

struct limeade_ack_raw
{
  uint16_t sz;
  void *data;
};

struct limeade_ack_recv_th_pass
{
  struct limeade_recv_data *in;
  struct limeade_ack_raw *out;
  pthread_mutex_t mtx;
};

void *limeade_th_await(void *arg)
{
  struct limeade_ack_recv_th_pass *data = (struct limeade_ack_recv_th_pass*)arg;
  pthread_mutex_lock(data->mtx);
  struct limeade_recv_data *r = data->in;
  data->out = (struct limeade_ack_raw*)malloc(sizeof(struct limead_ack_raw));

  pthread_mutex_lock(r->mtx_ack); // hangs

  data->out->sz = r->ack_sz;
  data->out->data malloc(data->out->sz);
  memcpy(data->out->data, r->ack, data->out->sz);

  pthread_mutex_unlock(r->mtx_ack);
  pthread_mutex_unlock(data->mtx);
  return NULL;
}

int limeade_send_await(struct limeade_context *ctx, enum limeade_packet type, ...)
{
  va_list arg;
  va_start(arg, type);

  int _;
  struct limeade_packet_data pkt;
  struct limeade_packet_flags flags;
  struct limeade_csm_compression_entry c_entry;
  struct limeade_csm_latency_entry l_entry;
  struct limeade_ack_recv_th_pass recv_ack;
  pthread_t recv_ack_tid;

  struct timespec compr_ts[2], latent_ts[2], sendts, recvwait, sleeprem;

  uint64_t iters, retries_iter, iter_giveup, retry_inc;

  _ = limeade_monotonic(&compr_ts[0]) 
  if(_ != LIMEADE_SUCCESS)
  {
    return _;
  }

  c_entry.compr_lvl = ctx->compr_lvl;

  pkt.type = flags.type = type;

  pthread_mutex_lock(ctx->mtx_compr);
  flags.compr_lvl = ctx->compr_lvl;
  pthread_mutex_unlock(ctx->mtx_compr);

  _ = limeade_populate_packet(&pkt, &flags, arg);
  if(_ != LIMEADE_SUCCESS)
  {
    return _;
  }

  c_entry.precompr_sz = pkt.pkt_sz;

  _ = limeade_deflate_packet(&pkt, flags.compr_lvl);
  if(_ != LIMEADE_SUCCESS)
  {
    return _;
  }

  c_entry.postcompr_sz = pkt.pkt_sz;
  l_entry.send_sz      = pkt.pkt_sz;

  _ = limeade_monotonic(&sendts);
  if(_ != LIMEADE_SUCCESS)
  {
    return _;
  }

  ((struct limeade_packet_flags*)pkt.flags)->packet_size = pkt->pkt_sz;
  ((struct limeade_packet_flags*)pkt.flags)->ts_s = sendts.tv_sec;
  ((struct limeade_packet_flags*)pkt.flags)->ts_ms = sendts.tv_nsec / 100;

  limeade_montonic(&compr_ts[1]);
  LIMEADE_CHECK();

  limeade_monotonic(&latent_ts[0]);
  LIMEADE_CHECK();

  limeade_base_send(ctx, &pkt);
  LIMEADE_CHECK();

  recv_ack.in = ctx->recv;
  pthread_mutex_init(&recv_ack.mtx);

  if(pthread_create(&recv_ack_tid, NULL, limeade_th_await, &recv_ack) != 0)
  {
    limeade_inserr(LIMEADE_ERROR_OTHER);
    goto err;
  }
  // TODO check ret

  recvwait.tv_sec  = 0;
  recvwait.tv_nsec = 1000000; // 1ms

  iters = 0;

  pthread_mutex_lock(ctx->mtx_pub);
  iter_giveup = ctx->ack_wait_time_ms;
  retry_iter = retry_inc = ctx->retry_interval;
  pthread_mutex_unlock(ctx->mtx_pub);

  // please note that I currently don't care if this is an accurate timer or not
  do
  {
    nanosleep(&recvwait, &sleeprem);
    iters++;
    if(iters >= iter_giveup)
    {
      limeade_inserr(LIMEADE_REJECTED);
      pthread_cancel(recv_ack_tid);
      goto err;
    } else if(iters >= retries_iter)
    {
      limeade_base_send(ctx, &pkt);
      retries_iter += retry_inc;
    }
  } while(pthread_mutex_trylock(&recv_ack.mtx) != EBUSY);

  limeade_monotonic(&latent_ts[1]);

  c_entry.elapsed_ms = limeade_monotonic_diff_ms(&compr_ts[0], &compr_ts[1]);
  l_entry.elapsed_ms = limeade_monotonic_diff_ms(&latent_ts[0], &latent_ts[1]);

  // TODO confirm that the acknowledgement is for this packet

  limeade_csm_add_compr_entry(ctx, &c_entry);
  limeade_csm_add_latency_entry(ctx, &l_entry);

  limeade_inserr(LIMEADE_SUCCESS);
  free(pkt.pkt);
  return 0;

  err:
  free(pkt.pkt);
  return -1;
}

#undef CLS_INTREPR
#undef CLS_DBLREPR
#undef WRS
#undef WRU
#undef WRI
#undef WRD
