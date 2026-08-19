// th_recv.c

#include<pthread.h>
#include<stdint.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<time.h>

#include<liblimeade/liblimeade.h>

void th_recv_client_eth(struct limeade_context *ctx)
{
  struct limeade_recv_data *r = &ctx->recv;
  struct limeade_indiv_recv *d;
  uint16_t wr_idx;
  // lock?
  struct timespec rmtp, rqtp = {0, 1000000000/r->hz};
  
  void *mag1, *mag2, *end, *interim = malloc(65535);
  int a, b;

  while(pthread_mutex_trylock(r->mtx_kys) == EBUSY)
  {
    b = 0;
    pthread_mutex_lock(ctx->mtx_rfd);
    pthread_mutex_lock(ctx->mtx_mode_union);
    do
    {
      a = recvfrom(ctx->rfd, interim + b, 1472, MSG_DONTWAIT, ctx->saddr, ctx->saddr_len);
      if(a <= 0)
      {
        if(errno == EAGAIN || errno == EWOULDBLOCK || a == 0)
        {
          // no more data
          break;
        }
        pthread_mutex_unlock(ctx->mtx_rfd);
        pthread_mutex_unlock(ctx->mtx_mode_union);
        goto err;
      }
      b += a;
    }
    pthread_mutex_unlock(ctx->mtx_rfd);
    pthread_mutex_unlock(ctx->mtx_mode_union);

    mag1 = interim;
    end  = interim + b;

    if(pthread_mutex_trylock(r->mtx_kys) != EBUSY)
    {
      break;
    }

    do
    {
      mag1 = memmem(mag1, end - mag1, &LIMEADE_MAGIC, MAGSZ);
      
      if(mag1 == NULL || mag1 == end - MAGSZ)
      {
        break;
      }
      
      mag2 = memmem(mag1 + MAGSZ, end - mag1 - MAGSZ, &LIMEADE_MAGIC, MAGSZ);
      
      if(mag2 == NULL)
      {
        mag2 = end;
      }
      
      if(((struct limeade_packet_flags*)mag1 + MAGSZ).type == LIMEADE_RECV)
      {
        // TODO optimize
        uint64_t pkt_sz = mag2 - mag1 - MAGSZ;
        free(r->ack);
        r->ack = malloc(pkt_sz);
        memcpy(r->ack, mag1 + MAGSZ, pkt_sz);
        pthread_mutex_unlock(r->mtx_ack);
        // if you forgot to wait, clearly it wasn't that important in the first
        // place
        pthread_mutex_lock(r->mtx_ack);
      } else
      {
        pthread_mutex_lock(r->mtx_idx);
        r->wr_idx++;
        if(r->wr_idx == r->nr_pkts)
        {
          r->wr_idx = 0;
        }
        d = r->pkts[r->wr_idx];
        pthread_mutex_unlock(r->mtx_idx);

        pthread_mutex_lock(d->mtx);
        d->sz = mag2 - mag1 - MAGSZ;
        memcpy(d->data, mag1 + MAGSZ, d->sz);
        d->flags = 0;
        pthread_mutex_unlock(d->mtx);
      }
      
      if(mag2 == end)
      {
        break;
      }

      mag1 = mag2 + MAGSZ;
    }
    nanosleep(&rqtp, &rmtp);
  }

  err:
  
  free(interim);
}

