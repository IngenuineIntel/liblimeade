// send.c
//
// [AGPL here]

#include <stdarg.h>

#include <zlib.h>

#include <liblimeade/liblimeade.h>

static void prep_str(char *in)
{
  for(char *i = in; i > NULL; i++)
  {
    if(*i == LIMEADE_FIELD_DELIM || *i == LIMEADE_ROW_DELIM)
    {
      *i = " ";
    } else if (*i == "\x00")
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
#define WRS(x)          \
if(x != NULL)           \
{                       \
  len = strlen(x);      \
  char tmp[len + 1];    \
  strcpy(&tmp, x);      \
  prep_str(&tmp);       \
  wr_at_idx(&tmp, len); \
}
#define WRU(x)                                     \
{                                                  \
  len = prep_uint(x, &int_repr, sizeof(int_repr)); \
  wr_at_idx(&int_repr, len);                       \
  CLS_INTREPR();                                   \
}
#define WRI(x)                                    \
{                                                 \
  len = prep_int(x, &int_repr, sizeof(int_repr)); \
  wr_at_idx(&int_repr, len);                      \
  CLS_INTREPR();                                  \
}
#define WRF(x)                                      \
{                                                   \
  len = prep_float(x, &dbl_repr, sizeof(dbl_repr)); \
  wr_at_idx(&dbl_repr, len);                        \
  CLS_DBLREPR();                                    \
}
#define FDELIM() wr_at_idx(&LIMEADE_FIELD_DELIM, 1);
#define RDELIM() wr_at_idx(&LIMEADE_ROW_DELIM, 1);
void limeade_populate_packet(struct limeade_packet_data *in, struct limeade_flags *flags, va_list arg)
{
  /* initializes `in`, and populates `*pkt` with appropriate packet data */

  unsigned int wr_idx = 0;
  unsigned int realloc_increemnt = 2048; // arbitrarily chosen value

  // memcpy wrapper + realloc for saftey
  void wr_at_idx(void *data, unsigned int len)
  {
    if (in->pkt_size < wr_idx + len)
    {
      in->pkt_size += realloc_increment + len;
      in->pkt = realloc(in->pkt, in->pkt_size);
      // TODO handle potential realloc error here
    }
    memcpy(in->pkt + wr_idx, data, len);
    wr_idx += len;
  }

  // buffers for int/float representation (because we do it a lot)
  char int_repr[18]; // 18 == len(hex(-2**63))-2+1 == len(hex(2**64))-2+1
  char dbl_repr[32]; // arbitrary value I chose
  int len; // reused often for string length/write length calculation

  in->pkt = malloc(realloc_increment * 2); // *2 is arbitrary atm
  
  // note on the packet flags:
  // flags.packet_size, flags.ts_s, & flags.ts_ms are all filled in retroactively,
  // & are not within the scope of this function
  memcpy(in->pkt, flags, sizeof(limeade_packet_flags));

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

  in->data = in->pkt + sizeof(limeade_packet_flags);
  in->pkt_sz = wr_idx + sizeof(limeade_packet_flags);

  // note: there is uninitialied memory sitting in the back of the buffer that
  // isn't being `realloc`'d here. This is because the data in the buffer might
  // have to be compressed, and it will be `realloc`'d (or just straight up
  // `free`d) eventually anyway
  // the true size of the buffer is lost, but it is not needed; we do not lose
  // the used size of the buffer

  // TODO make this function do error checking
  limeade_inserror(LIMEADE_SUCCESS);
}

void limeade_send_base(struct limeade_context *ctx, struct limeade_packet_data *pkt)
{
  /* sends packet */
  switch(ctx->mode)
  {
    case LIMEADE_MODE_HOST_SSH:
    {
      if(write(ctx->sfd, pkt->pkt, pkt->pkt_sz) != pkt->pkt_sz)
      {
        limeade_inserr(LIMEADE_ERROR_NETWORKING);
      } else
      {
        limeade_inserr(LIMEADE_SUCCESS);
      }
    }
    case LIMEADE_MODE_CLIENT_SSH:
    {
      if(write(ctx->sfd, pkt->pkt, pkt->pkt_sz) != pkt->pkt_sz)
      {
        limeade_inserr(LIMEADE_ERROR_NETWORKING);
      } else
      {
        limeade_inserr(LIMEADE_SUCCESS);
      }
    }
    case LIMEADE_MODE_CLIENT_LIBSSH:
    {}
    case LIMEADE_MODE_HOST_ETH:
    {}
    case LIMEADE_MODE_CLIENT_ETH:
    {}
  }
}

#define CHECK(err)       \
e = limeade_poperr();    \
if(e != LIMEADE_SUCCESS) \
{                        \
  limeade_inserr(e);     \
  free(pkt->pkt);        \
  return -1;             \
}
int limeade_send(struct limeade_context *ctx, limeade_packet type, ...)
{

  va_list arg;
  va_start(arg, type);
  
  LIMEADE_ERROR e;

  struct limeade_packet_data pkt;
  struct limeade_packet_flags flags;
  struct limeade_csm_compression_entry entry;
  
  struct timespec compr_ts[2], sendts;

  limeade_monotonic(&compr_ts[0]);
  CHECK();

  entry.compr_lvl = ctx->compr_lvl;

  pkt.type = flags.type = type;
  
  pthread_mutex_lock(&ctx->compr_lvl_mtx);
  flags.compr_lvl = ctx->compr_lvl;
  pthread_mutex_unlock(&ctx->compr_lvl_mtx);

  limeade_populate_packet(&pkt, &flags, arg);
  CHECK();

  entry.precompr_sz = pkt.pkt_sz;

  limeade_deflate_packet(&pkt, flags.compr_lvl);
  CHECK();

  entry.postcompr_sz = pkt.pkt_sz;

  limeade_monotonic(&sendts);
  CHECK();

    // filling in flags
  ((struct limeade_packet_flags*)pkt.flags)->packet_size = pkt->pkt_sz;
  ((struct limeade_packet_flags*)pkt.flags)->ts_s  = sendts.tv_sec;
  ((struct limeade_packet_flags*)pkt.flags)->ts_ms = sendts.tv_nsec / 1000;

  limeade_monotonic(&compr_ts[1]);
  CHECK();

  limeade_base_send(ctx, &pkt);
  CHECK();

  entry.elapsed_ms = lime_monotonic_diff_ms(&compr_ts[0], &compr_ts[1]);

  limeade_csm_add_compr_entry(ctx, &entry);
  CHECK();

  limeade_inserror(LIMEADE_SUCCESS);
  free(pkt.pkt);
  return 0;
}

#undef CLS_INTREPR
#undef CLS_DBLREPR
#undef WRS
#undef WRU
#undef WRI
#undef WRD
#undef FDELIM
#undef RDELIM
#undef CHECK
