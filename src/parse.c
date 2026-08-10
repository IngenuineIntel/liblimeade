// parse.c
// receiving and parsing
// [AGPL]

#include<pthread.h>
#include<stdlib.h>
#include<string.h>

#include<liblimeade/liblimeade.h>

static int limeade_recvfrom_safe(int sockfd, void *buf, size_t size, int flags)
{
  // counting works differently when MSG_PEEK is used
  int ret = 0;
  if (flags == flags & (~0 - MSG_PEEK))
  {
    do
    {
      ret = recvfrom(sockfd, buf+ret, size-ret, flags);
      if (ret < 1)
      {
        return ret;
      }
    } while(ret != size);
  } else
  {
    do
    {
      ret += recvfrom(sockfd, buf, size, flags);
      if (ret < 1)
      {
        return ret;
      }
    } while (ret != size);
  return ret;
}

#define MAGSZ sizeof(LIMEADE_MAGIC)
#define F (struct limeade_packet_flags*)ret.pkt
#define RECV(x, y, z)\
{\
  len = limeade_recvfrom_safe(ctx->rfd, x, y, z, &ctx->saddr);\
  if(len < 1 && y > len)\
  {\
    free(tmp_magic);\
    free(ret.pkt);\
    limeade_inserror(LIMEADE_ERROR_NO_DATA ? len == 0 : LIMEADE_ERROR_NETWORK);\
    return ret;\
  }\
}
#define READ(x, y)\
{\
  len = read(ctx->rfd, x, y);\
  if (len != y)\
  {\
    free(tmp_magic);\
    free(ret.pkt);\
    limeade_inserr(LIEMADE_ERROR_NO_DATA);\
    return ret;\
  }
struct limeade_recv limeade_recv_wait(struct limeade_context *ctx)
{
  struct limeade_recv ret;
  int len;

  void *tmp_magic = malloc(MAGSZ);
  ret.pkt = malloc(FLAGSZ);

  if(ctx->mode == LIMEADE_MODE_HOST_SSH || ctx->mode == LIMEADE_MODE_CLIENT_SSH)
  {
    // when reading in this mode, we don't have a MSG_PEEK-like option when
    // managing garbage
    // therefore, we have to read in chunks until we find part of the magic
    // and/or flags, then copy the useful data to another buffer, and continue
    // to read the packet into that buffer at an offset

    READ(tmp_magic, MAGSZ);
    // assuming the stream isn't borked in order to offset the performance
    // downgrade to solely the edge case
    if(memcmp(tmp_magic, &LIMEADE_MAGIC, MAGSZ) != 0)
    {

    }


  } else if (ctx->LIMEADE_MODE_HOST_ETH || ctx->mode == LIMEADE_MODE_CLIENT_ETH)
  {
    int flags = MSG_WAITALL;
    RECV(tmp_magic, MAGSZ, flags|MSG_PEEK);

i   if(memcmp(tmp_magic, &LIMEADE_MAGIC, MAGSZ) != 0)
    {
      // there is garbage in the queue
      // we assume there will be a magic eventually, so we look for it & clear
      // the garbage data
      tmp_magic = realloc(tmp_magic, MAGSZ * 5);
      char *magic_inst;
      do
      {

      }
    } else
    {
      // just to clear the magic from the queue
      RECV(tmp_magic, MAGSZ, flags);
    }

    free(tmp_magic);

    ret.pkt = malloc(sizeof(struct limeade_packet_flags));

    RECV(ret.pkt, sizeof(struct limeade_packet_flags), flags);

    ret.type   = F->type;
    ret.compr  = F->compr_lvl;
    ret.pkt_sz = F->packet_size;
    ret.pkt    = realloc(ret.pkt, F->packet_size);
    ret.data   = ret.pkt + sizeof(struct limeade_packet_flags);

    RECV(ret.data, ret.pkt_sz - sizeof(struct limeade_packet_flags, flags));

    return ret;

  } else if (ctx->LIMEADE_MODE_CLIENT_LIBSSH)
  {
    // TODO
  } else
  {
    limeade_inserr(LIMEADE_ERROR_INVALID_CONTEXT);
    return ret;
  }
}

struct limeade_recv limeade_recv_fixed(struct limeade_context *ctx, int hold_time_s)
{
  // TODO
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

