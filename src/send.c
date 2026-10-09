// send.c

#include<errno.h> // IWYU pragma: keep
#include<math.h>  // isnan isinf
#include<pthread.h>
#include<semaphore.h>
#include<stdarg.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>

#include<zlib.h>

#include<liblimeade/liblimeade-internal.h>

LIMEADE_GEN_PREP_FN(prep_u8,    uint8_t,  LIMEADE_UINT_INDICATOR);
LIMEADE_GEN_PREP_FN(prep_u16,   uint16_t, LIMEADE_UINT_INDICATOR);
LIMEADE_GEN_PREP_FN(prep_u32,   uint32_t, LIMEADE_UINT_INDICATOR);
LIMEADE_GEN_PREP_FN(prep_u64,   uint64_t, LIMEADE_UINT_INDICATOR);
LIMEADE_GEN_PREP_FN(prep_i8,    int8_t,   LIMEADE_SINT_INDICATOR);
LIMEADE_GEN_PREP_FN(prep_i16,   int16_t,  LIMEADE_SINT_INDICATOR);
LIMEADE_GEN_PREP_FN(prep_i32,   int32_t,  LIMEADE_SINT_INDICATOR);
LIMEADE_GEN_PREP_FN(prep_i64,   int64_t,  LIMEADE_SINT_INDICATOR);
LIMEADE_GEN_PREP_FN(prep_float, float,    LIMEADE_FLT_INDICATOR);

static int prep_double(double in, void *out, int max)
{
  if(!out) return 0;
  if(sizeof(in) + 1 > max) return -1;
  if(isnan(in) || isinf(in))
  {
    *(uint8_t*)out = '\x00';
    return 1;
  }
  *(uint8_t*)out = LIMEADE_FLT_INDICATOR | (sizeof(in) - 1);
  *((double*)((uint8_t*)out + 1)) = in;
  return sizeof(in) + 1;
}

static int prep_str(const char *in, void *out, int max)
{
  if(!in || !out) return 0;
  int r = strlen(in) + 1;
  if(r > max) return -1;
  memcpy(out, in, r);
  for(char *i = out; *i != '\0'; i++)
    if(*i == LIMEADE_FIELD_DELIM || *i == LIMEADE_ROW_DELIM)
      *i = ' ';
  return --r;
}

#define ins_v(in, out, max) _Generic((in),\
  uint8_t: prep_u8,        \
  uint16_t: prep_u16,      \
  uint32_t: prep_u32,      \
  uint64_t: prep_u64,      \
  int8_t: prep_i8,         \
  int16_t: prep_i16,       \
  int32_t: prep_i32,       \
  int64_t: prep_i64,       \
  float: prep_float,       \
  double: prep_double,     \
  char*: prep_str          \
)(in, out, max)

void limeade_populate_packet(struct limeade_packet_data *in,
                             struct limeade_packet_flags *flags, va_list arg)
{
  register int s = 65536 + sizeof(LIMEADE_MAGIC);
  int m   = s;
  void *b = malloc(s);
  in->pkt = b;

  memcpy(b, LIMEADE_MAGIC, sizeof(LIMEADE_MAGIC));
  b += sizeof(LIMEADE_MAGIC);
  memcpy(b, flags, sizeof(*flags));
  b += sizeof(*flags);
  s -= sizeof(*flags) + sizeof(LIMEADE_MAGIC);

#define INC(in)\
{\
  int ret = ins_v(in, b, s);\
  if(ret < 0) goto err;\
  b += ret;\
  s -= ret;\
}
#define FDELIM() *(char*)b = LIMEADE_FIELD_DELIM; b++; s--;
#define RDELIM() *(char*)b = LIMEADE_ROW_DELIM; b++; s--;

  switch(in->type)
  {
    case LIMEADE_PACKET_KNOCK:
    {
      struct limeade_knock data = va_arg(arg, struct limeade_knock);

      INC(data.prev_connected);
      FDELIM();
      INC(data.prev_session);
      RDELIM();
      break;
    }

    case LIMEADE_PACKET_RECOGNIZE:
    {
      struct limeade_recognize data = va_arg(arg, struct limeade_recognize);

      INC(data.accepted);
      FDELIM();
      INC(data.new_session);
      RDELIM();
      break;
    }

    case LIMEADE_PACKET_INTRODUCTION:
    {
      struct limeade_introduction data = va_arg(arg, struct limeade_introduction);

      INC(data.hostname);
      FDELIM();
      INC(data.kernelver);
      FDELIM();
      INC(data.distro);
      FDELIM();
      INC(data.origin_user);
      FDELIM();
      INC(data.processor);
      FDELIM();
      INC(data.vendor);
      FDELIM();
      INC(data.ram_mbs);
      FDELIM();
      INC(data.swap_mbs);
      RDELIM();
      break;
    }

    case LIMEADE_PACKET_ACKNOWLEDGE:
    {
      struct limeade_acknowledge data = va_arg(arg, struct limeade_acknowledge);

      INC(data.send_ts_s);
      FDELIM();
      INC(data.send_ts_ms);
      FDELIM();
      INC(data.recv_ts_s);
      FDELIM();
      INC(data.recv_ts_ms);
      RDELIM();
      break;
    }

    case LIMEADE_PACKET_EVENTS:
    {
      struct limeade_events data = va_arg(arg, struct limeade_events);
      struct limeade_indiv_event *j;

      INC(data.nr_events);
      RDELIM();

      for(int i = 0; i < data.nr_events; i++)
      {
        j = &data.events[i];

        INC(j->ts_s);
        FDELIM();
        INC(j->ts_ms);
        FDELIM();
        INC(j->pid);
        FDELIM();
        INC(j->syscall);
        FDELIM();
        INC(j->arg1);
        FDELIM();
        INC(j->arg2);
        FDELIM();
        INC(j->retval);
        RDELIM();
      }
      break;
    }

    case LIMEADE_PACKET_PROC_GENERIC:
    {
      struct limeade_proc_generic data = va_arg(arg, struct limeade_proc_generic);
      struct limeade_indiv_proc *j;

      for(int i = 0; i < data.total; i++)
      {
        j = &data.procs[i];

        INC(j->pid);
        FDELIM();
        INC(j->ppid);
        FDELIM();
        INC(j->uid);
        FDELIM();
        INC(j->threads);
        FDELIM();
        INC(j->cpu_ticks);
        FDELIM();
        INC(j->ram_kb);
        FDELIM();
        INC(j->command);
        RDELIM();
      }
      break;
    }

    case LIMEADE_PACKET_PROC_UPDATE:
    {
      struct limeade_proc_update data = va_arg(arg, struct limeade_proc_update);
      struct limeade_indiv_proc *j;
      int i;

      INC(data.total_died);
      FDELIM();
      INC(data.total_altered);
      RDELIM();

      data.total_died--;
      for(i = 0; i < data.total_died; i++)
      {
        INC(data.died[i]);
        FDELIM();
      }
      INC(data.died[data.total_died]);
      RDELIM();

      //data.total_died++;

      for(i = 0; i < data.total_altered; i++)
      {
        j = &data.altered[i];

        INC(j->pid);
        FDELIM();
        INC(j->ppid);
        FDELIM();
        INC(j->uid);
        FDELIM();
        INC(j->threads);
        FDELIM();
        INC(j->cpu_ticks);
        FDELIM();
        INC(j->ram_kb);
        FDELIM();
        INC(j->command);
        RDELIM();
      }
      break;
    }

    case LIMEADE_PACKET_PERF:
    {
      struct limeade_perf data = va_arg(arg, struct limeade_perf);

      INC(data.cores);
      FDELIM();
      INC(data.avg_cpu_pct);
      FDELIM();
      INC(data.mem_total_kb);
      FDELIM();
      INC(data.mem_free_kb);
      FDELIM();
      INC(data.mem_available_kb);
      FDELIM();
      INC(data.mem_cached_kb);
      FDELIM();
      INC(data.load_1m);
      FDELIM();
      INC(data.load_5m);
      FDELIM();
      INC(data.load_15m);
      FDELIM();
      INC(data.other);
      RDELIM();
      break;
    }

    case LIMEADE_PACKET_COMMANDEER:
    {
      struct limeade_commandeer data = va_arg(arg, struct limeade_commandeer);

      INC(data.command);
      FDELIM();
      INC(data.flags);
      FDELIM();
      INC(data.id);
      RDELIM();
      break;
    }

    case LIMEADE_PACKET_EXITED:
    {
      struct limeade_exited data = va_arg(arg, struct limeade_exited);

      INC(data.id);
      FDELIM();
      INC(data.exitcode);
      RDELIM();
      break;
    }

    case LIMEADE_PACKET_CLOSE:
    {
      struct limeade_close data = va_arg(arg, struct limeade_close);

      INC(data.explanation);
      RDELIM();
      break;
    }
  }

  in->pkt_sz = m - s;
  in->flags  = in->pkt   + sizeof(LIMEADE_MAGIC);
  in->data   = in->flags + sizeof(struct limeade_packet_flags);

  return;

err:
  free(in->pkt);
  return;
#undef INC
#undef FDELIM
#undef RDELIM
}

int limeade_deflate_packet(struct limeade_packet_data *pkt, int compr_lvl)
{
  if(compr_lvl <= 0)
    return LIMEADE_SUCCESS;

  unsigned int len = pkt->pkt_sz - sizeof(struct limeade_packet_flags) - sizeof(LIMEADE_MAGIC);
  uLongf compressed_len = compressBound(len);
  void *compressed = malloc(compressed_len);
  if(!compressed)
    return LIMEADE_ERROR_MEMORY;

  if(compress2(compressed, &compressed_len, pkt->data, len, compr_lvl) != Z_OK)
  {
    free(compressed);
    return LIMEADE_ERROR_COMPRESSION;
  }

  // in case compression would be inefficient
  if(compressed_len >= len)
  {
    free(compressed);
    ((struct limeade_packet_flags*)pkt->flags)->compr_lvl = 0;
    return LIMEADE_SUCCESS;
  }

  memcpy(pkt->data, compressed, compressed_len);
  free(compressed);

  pkt->pkt_sz = compressed_len
              + sizeof(struct limeade_packet_flags)
              + sizeof(LIMEADE_MAGIC);
  
  void *new_pkt = realloc(pkt->pkt, compressed_len
                                  + sizeof(struct limeade_packet_flags)
                                  + sizeof(LIMEADE_MAGIC));
  if(!new_pkt)
    return LIMEADE_ERROR_MEMORY;

  pkt->pkt   = new_pkt;
  pkt->flags = new_pkt    + sizeof(LIMEADE_MAGIC);
  pkt->data  = pkt->flags + sizeof(struct limeade_packet_flags);

  return LIMEADE_SUCCESS;
}

int limeade_send_base(struct limeade_context *ctx, struct limeade_packet_data *pkt)
{
  pthread_mutex_lock(&ctx->mtx_sfd);

  if(ctx->mode == LIMEADE_MODE_HOST_SSH || ctx->mode == LIMEADE_MODE_CLIENT_SSH)
    write(ctx->sfd, pkt->pkt, pkt->pkt_sz);

  else if(ctx->mode == LIMEADE_MODE_CLIENT_ETH)
  {
    pthread_mutex_lock(&ctx->mtx_mode_specific);

    sendto(ctx->sfd, pkt->pkt, pkt->pkt_sz, 0,
          (struct sockaddr*)&ctx->saddr, sizeof(ctx->saddr));

    pthread_mutex_unlock(&ctx->mtx_mode_specific);

  } else if (ctx->mode == LIMEADE_MODE_HOST_ETH)
  {
    pthread_mutex_lock(&ctx->mtx_mode_specific);

    sendto(ctx->sfd, pkt->pkt, pkt->pkt_sz, 0,
           (struct sockaddr*)&ctx->cliaddr, ctx->cliaddr_len);

    pthread_mutex_unlock(&ctx->mtx_mode_specific);

  } else if(ctx->mode == LIMEADE_MODE_CLIENT_LIBSSH)
  {
    pthread_mutex_unlock(&ctx->mtx_sfd);
    return LIMEADE_ERROR_NOT_SUPPORTED;

  } else
  {
    pthread_mutex_unlock(&ctx->mtx_sfd);
    return LIMEADE_ERROR_INVALID_CONTEXT;
  }

  pthread_mutex_unlock(&ctx->mtx_sfd);  
  return LIMEADE_SUCCESS;
}

int limeade_send(struct limeade_context *ctx, enum limeade_packet type, ...)
{
  register int _;
  struct limeade_packet_data pkt;
  struct limeade_packet_flags flags, *f;
  struct limeade_csm_compression_entry entry;
  struct timespec compr_ts[2], send_ts;
  va_list arg;
  va_start(arg, type);

  memset(&flags, 0, sizeof(flags));

  _ = limeade_statecheck(ctx);
  if(_ != LIMEADE_SUCCESS)
    return _;

  _ = limeade_monotonic(&compr_ts[0]);
  if(_ != LIMEADE_SUCCESS)
    return _;

  entry.compr_lvl = ctx->compr_lvl;

  pkt.type = flags.type = type;

  pthread_mutex_lock(&ctx->mtx_compr_lvl);
  flags.compr_lvl = ctx->compr_lvl;
  pthread_mutex_unlock(&ctx->mtx_compr_lvl);

  limeade_populate_packet(&pkt, &flags, arg);

  entry.precompr_sz = pkt.pkt_sz;

  _ = limeade_deflate_packet(&pkt, flags.compr_lvl);
  if(_ != LIMEADE_SUCCESS)
    goto err;

  entry.postcompr_sz = pkt.pkt_sz;

  _ = limeade_monotonic(&send_ts);
  if(_ != LIMEADE_SUCCESS)
    goto err;

  f = pkt.flags;
  
  f->packet_size = pkt.pkt_sz;
  f->ts_s        = send_ts.tv_sec;
  f->ts_ms       = send_ts.tv_nsec / 100;

  _ = limeade_monotonic(&compr_ts[1]);
  if(_ != LIMEADE_SUCCESS)
    goto err;

  _ = limeade_send_base(ctx, &pkt);
  if(_ != LIMEADE_SUCCESS)
    goto err;

  entry.elapsed_ms = limeade_monotonic_diff_ms(&compr_ts[0], &compr_ts[1]);

  limeade_csm_add_compr_entry(ctx, &entry);

  _ = LIMEADE_SUCCESS;

err:
  free(pkt.pkt);
  return _;
}

int limeade_send_await(struct limeade_context *ctx, enum limeade_packet type, ...)
{
  register int _;
  struct limeade_recv_data *r;
  struct limeade_packet_data pkt, recv_ack;
  struct limeade_packet_flags flags, *f, *ack_f;
  struct limeade_csm_compression_entry c_entry;
  struct limeade_csm_latency_entry l_entry;
  struct limeade_ack a;
  struct timespec compr_ts[2], latent_ts[2], sendts, recvwait;
  va_list arg;
  va_start(arg, type);

  r = &ctx->recv;

  memset(&flags, 0, sizeof(flags));

  _ = limeade_statecheck(ctx);
  if(_ != LIMEADE_SUCCESS)
    return _;

  _ = limeade_monotonic(&compr_ts[0]);
  if(_ != LIMEADE_SUCCESS)
    return _;

  c_entry.compr_lvl = ctx->compr_lvl;

  pkt.type = flags.type = type;

  pthread_mutex_lock(&ctx->mtx_compr_lvl);
  flags.compr_lvl = ctx->compr_lvl;
  pthread_mutex_unlock(&ctx->mtx_compr_lvl);

  limeade_populate_packet(&pkt, &flags, arg);

  c_entry.precompr_sz = pkt.pkt_sz;

  _ = limeade_deflate_packet(&pkt, flags.compr_lvl);
  if(_ != LIMEADE_SUCCESS)
    goto err;

  c_entry.postcompr_sz = pkt.pkt_sz;
  l_entry.send_sz      = pkt.pkt_sz;

  _ = limeade_monotonic(&sendts);
  if(_ != LIMEADE_SUCCESS)
    goto err;

  f = pkt.flags;
  f->packet_size = pkt.pkt_sz;
  f->ts_s        = sendts.tv_sec;
  f->ts_ms       = sendts.tv_nsec / 1000000;

  _ = limeade_monotonic(&compr_ts[1]);
  if(_ != LIMEADE_SUCCESS)
    goto err;

  _ = limeade_monotonic(&latent_ts[0]);
  if(_ != LIMEADE_SUCCESS)
    goto err;

  _ = limeade_send_base(ctx, &pkt);
  if(_ != LIMEADE_SUCCESS)
    goto err;

  pthread_mutex_lock(&ctx->mtx_pub_attrs);

  clock_gettime(CLOCK_REALTIME, &recvwait);
  recvwait.tv_sec  += ctx->ack_wait_time_ms / 1000;
  recvwait.tv_nsec += ctx->ack_wait_time_ms % 1000 * 1000000;
  
  pthread_mutex_unlock(&ctx->mtx_pub_attrs);

  retry:

  if(pthread_mutex_timedlock(&r->ack.mtx, &recvwait) != 0)
  {
    _ = LIMEADE_ERROR_REJECTED;
    goto err;
  }

  {
    char ack_pkt[r->ack.sz];
    recv_ack.pkt    = ack_pkt;
    recv_ack.flags  = ack_f = recv_ack.pkt + sizeof(LIMEADE_MAGIC);
    recv_ack.data   = recv_ack.flags + sizeof(*ack_f);
    recv_ack.pkt_sz = r->ack.sz;
    recv_ack.type   = ack_f->type;
    recv_ack.compr  = ack_f->compr_lvl;

    memcpy(ack_pkt, r->ack.data, r->ack.sz);

    _ = limeade_parse_ack(&a, &recv_ack);
  }

  if(_ != LIMEADE_SUCCESS)
    goto err;

  if(a.send_ts_ms != f->ts_ms
  || a.send_ts_s  != f->ts_s)
    goto retry;

  c_entry.elapsed_ms = limeade_monotonic_diff_ms(&compr_ts[0], &compr_ts[1]);
  l_entry.elapsed_ms = limeade_monotonic_diff_ms(&latent_ts[0], &latent_ts[1]);

  limeade_csm_add_compr_entry(ctx, &c_entry);
  limeade_csm_add_latency_entry(ctx, &l_entry);

  _ = LIMEADE_SUCCESS;

  err:

  free(pkt.pkt);
  return _;
}

