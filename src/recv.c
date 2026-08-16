// th_recv.c

struct limeade_th_csm_data;

struct limeade_indiv_recv
{
  void *data;
  uint16_t sz;
  void *mtx;
  union
  {
    uint8_t flags;

    struct
    {
      uint8_t has_been_read:1;
      uint8_t reserved:7;
    };
  };
};

struct limeade_th_recv_data
{
  tid_t tid;
  uint16_t nr_pkts;
  uint16_t read_idx;
  uint16_t wr_idx;
  uint16_t pkts_lost;
  uint16_t hz;
  struct limeade_indiv_recv **pkts;
  void *mtx_idx;
  void *mtx_kys;
  void *mtx_lost;
};

struct limeade_context
{
  int mode;
  int compr_lvl;

  int sfd;
  int rfd;

  union
  {
    pid_t ssh_pid;   // LIMEADE_MODE_CLIENT_SSH

    void **ssh_data; // LIMEADE_MODE_CLIENT_LIBSSH
  
    struct           // LIMEADE_MODE_HOST_ETH
    {
      uint32_t nr_clients;
      struct limeae_eth_host_indiv_client **clients;
    };

    struct           // LIMEADE_MODE_CLIENT_ETH
    {
      struct sockaddr_in *saddr;
      socketlen_t saddr_len;
    };
  };

  // thread data

  struct limeade_th_csm_data csm;
  struct limeade_th_recv_data recv;

  // "public attributes"
  char *destination;
  int port;
  uint64_t sessionid;

  // mutexes
  void *mtx_sfd;
  void *mtx_rfd;
  void *mtx_mode_union;
  void *mtx_csm;
  void *mtx_recv;
  void *mtx_public_attrs;
}

void th_recv_client_eth(struct limeade_context *ctx)
{
  struct limeade_recv_data *r = &ctx->recv;
  struct limeade_indiv_recv *d;
  uint16_t wr_idx;

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
      
      pthread_mutex_lock(r->mtx_idx);
      r->wr_idx++;
      if(r->wr_idx == r->nr_pkts)
      {
        r->wr_idx = 0;
      }
      d = r->pkts[r->wr_idx];
      pthread_mutex_unlock(r->mtx_idx);

      pthread_mutex_lock(d->mtx);
      d->sz = mag2 - mag1;
      memcpy(d->data, mag1 + MAGSZ, d->sz);
      d->flags = 0;
      pthread_mutex_unlock(d->mtx);

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

