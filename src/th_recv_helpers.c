// th_recv_helpers.c
//
// AGPL

#include<liblimeade/liblimeade-interna.h>

#define MAGSZ sizeof(LIMEADE_PACKET_MAGIC)

void limeade_th_recv_wr_pkt(struct limeade_recv_data *r, void *pkt,
                            unsigned int sz, struct sockaddr_in *addr,
                            socklen_t len)
{
  /* the most incredible docstirng you've ever read */

  if(((struct limeade_packet_flags*)pkt)->type == LIMEADE_ACKNOWLEDGE)
  {
    memcpy(r->ack, pkt, sz);

    if(addr)
    {
      memcpy(&r->ack_addr, addr, sizeof(*addr));
      r->ack_addr_len = len;
    }

    // LIMEADE_ACK packets are awaited by awaiting the unlock of r->mtx_ack. If
    // the main thread doesn't wait, it never knows if the peer sent an ACK
    pthread_mutex_unlock(r->mtx_ack);
    pthread_mutex_lock(r->mtx_ack);
  } else
  {
    pthread_mutex_lock(r->mtx_idx);
    
    r->wr_idx++;
    if(r->wr_idx >= r->nt_pkts)
      r->wr_idx = 0;

    register struct limeade_indiv_recv *d = &r->pkts[r->wr_idx];
    pthread_mutex_unlock(r->mtx_idx);

    pthread_mutex_lock(d->mtx);
    memcpy(d->data, pkt, sz);

    if(addr)
    {
      memcpy(&d->addr, addr, sizeof(*addr));
      d->addr_len = len;
    }

    if(d->has_been_read != 0)
    {
      pthread_mutex_lock(r->mtx_lost);
      r->pkts_lost++;
      pthread_mutex_unlock(r->mtx_lost);
    }
    d->flags = 0;

    pthread_mutex_unlock(d->mtx);
  }

}


inline int limeade_eth_recv(const struct limeade_context *ctx,
                                 const void *buffer, const unsigned int sz,
                                 struct sockaddr *cliaddr, socklen_t *cli_len)
{
  pthread_mutex_lock(ctx->mtx_rfd);
  pthread_mutex_lock(ctx->mtx_mode_union);

  int ret = recvfrom(ctx->rfd, buffer, sz, 0, cliaddr, cli_len);

  pthread_mutex_unlock(ctx->mtx_rfd);
  pthread_mutex_unlock(ctx->mtx_mode_union);

  return ret;
}

inline int limeade_client_ssh_recv(const struct limeade_context *ctx,
                                   const void *buffer, const unsigned int sz)
{
  pthread_mutex_lock(ctx->mtx_rfd);
  int ret = read(ctx->rfd, buffer, sz);
  pthread_mutex_unlock(ctx->mtx_rfd);
  return ret;
}


int limeade_prelim_confirm(const void *buffer, const int sz)
{
  /* Does preliminary checks on the packet in the buffer to confirm it's not
   * complete garbage. */

  // note: `return -1` is replaced with `goto f` because the assembly will look
  // nicer

  struct limeade_packet_flags *f = buffer + MAGSZ;

  if(sz <= MAGSZ + sizeof(*f) + 2)
    goto f;
  
  if(memcmp(buffer, &LIMEADE_MAGIC, MAGSZ) != 0)
    goto f;
  
  if(f->type >= LIMEADE_PACKET_MAX)
    goto f;

  if(f->pkt_sz > sz - MAGSZ)
    goto f;  

  return 0;
f:
  return -1;
}

