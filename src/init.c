// init.c

#include<pthread.h>
#include<stdarg.h>
#include<stdlib.h>

#include<liblimeade/liblimeade.h>

// guide to reading this file:
// Each other function is either a constructor or a destructor, &
// constructors can sometimes be a `step_1` or a `step_2`. Step 1
// constructors (or constructors that aren't multiple steps) are
// called within limeade_init, and step 2 constructors are called
// within limeade_connect. This way, any/all default settings from
// limeade_init can be customized by the user before limeade_connect,
// in which case such is not always possible. All *_destruct_*
// functions (deconstructors) are called in limeade_release. 
// Destructors can also be in two steps. This is because, theoretically,
// the second step can fail, in which case the first step can be
// destructed cleanly.
// limeade_init, limeade_connect, & limeade_release are all at the
// bottom of this file.

// note to editors of this file: make constructors inline, don't make
// deconstructors inline, because they are called in more logic
// keep constructors inlined, since (& make sure of this, too) they are only
// called once, as they only exist to move logic away from high-level functions

inline int limeade_init_mutexes(struct limeade_context *ctx)
{
  /* populates mtx_sfd, mtx_rfd, mtx_mode_union, mtx_compr,
   * mtx_th_csm, & mtx_pub */

  pthread_mutex_t *mutexes = malloc(sizeof(pthread_mutex_t) * 6);
  if(!mutexes)
    return LIMEADE_ERROR_MEMORY;

  for(int i = 0; i < 6; i++)
    pthread_mutex_init(&mutexes[i]);

  ctx->mtx_sfd        = &mutexes[0];
  ctx->mtx_rfd        = &mutexes[1];
  ctx->mtx_mode_union = &mutexes[2];
  ctx->mtx_compr      = &mutexes[3];
  ctx->mtx_th_csm     = &mutexes[4];
  ctx->mtx_pub        = &mutexes[5];

  return LIMEADE_ERROR_SUCCESS;
}

void limeade_destruct_mutexes(struct limeade_context *ctx)
{
  /* releases mtx_sfd, mtx_rfd, mtx_mode_union, mtx_compr,
   * mtx_th_csm, & mtx_pub */

  // this looks janky; however, `ctx->mtx_sfd` is _technically_ of
  // type `pthread_mutex_t[6]`
  for(int i = 0; i < 6; i++)
    pthread_mutex_destroy(&ctx->mtx_sfd[i]);

  free(ctx->mtx_sfd);
}

inline int limeade_init_th_recv_step_1(struct limeade_context *ctx)
{
  /* populates ctx->recv (step 1) */

  struct limeade_recv_data *r;
  pththread_mutex_t *mutexes;
  struct limeade_indiv_recv *d;

  r = malloc(sizeof(struct limeade_recv_data));
  if(!r)
    return LIMEADE_ERROR_MEMORY;

  ctx->recv = r;

  // mutexes
  // this allocator is 4 mutexes, then a pthread_t at the end
  mutexes = malloc(sizeof(pthread_mutex_t) * 4 + sizeof(pthread_t));
  if(!mutexes)
  {
    free(r);
    return LIMEADE_ERROR_MEMORY;
  }

  for(int i = 0; i < 4; i++)
    pthread_mutex_init(&mutexes[i]);

  r->mtx_ack  = &mutexes[0];
  r->mtx_idx  = &mutexes[1];
  r->mtx_kys  = &mutexes[2];
  r->mtx_lost = &mutexes[3];
  r->tid      = (pthread_t*)&mutexes[4];

  r->nr_pkts   = LIMEADE_NR_PKTS_DEFAULT;
  r->read_idx  = 0;
  r->wr_idx    = 0;
  r->pkts_lost = 0;
  r->hz        = LIMEADE_RECV_DEFAULT_HZ;
  r->ack_sz    = 65535; // max packet size (though a proper LIMEADE_ACK could
                        // never be morethan 1KB)

  return LIMEADE_SUCCESS;
}

inline int limeade_init_th_recv_step_2(struct limeade_context *ctx)
{
  /* ctx->recv (step 2) */
  struct limeade_indiv_recv *d;
  struct limeade_recv_data  *r = ctx->recv;

  void*(*th_recv_f)(void*);

  switch(ctx->mode)
  {
    case LIMEADE_MODE_CLIENT_SSH:
      th_recv_f = &limeade_th_recv_client_ssh;
    case LIMEADE_MODE_HOST_SSH:
      th_recv_f = &limeade_th_recv_host_ssh;
    case LIMEADE_MODE_LIBSSH:
#ifdef LIMEADE_HAS_SSH
      th_recv_f = &limeade_th_recv_libssh;
#else
      return LIMEADE_ERROR_NOT_SUPPORTED;
#endif
    case LIMEADE_MODE_CLIENT_ETH:
      th_recv_f = &limeade_th_recv_client_eth;
    case LIMEADE_MODE_HOST_ETH:
      th_recv_f = &limeade_th_recv_host_eth;
  }

  r->pkts = malloc(sizeof(struct limeade_indiv_recv) * r->nr_pkts);
  if(!r->pkts)
    return LIMEADE_ERROR_MEMORY;

  r->ack = malloc(r->ack_sz);
  if(!r->ack)
  {
    free(r->pkts);
    return LIMEADE_ERROR_MEMORY;
  }

  for(int i = 0; i < r->nr_pkts; i++)
  {
    d = &r->pkts[i];

    d->sz    = 0;
    d->flags = 0;
    d->mtx   = malloc(sizeof(pthread_mutex_t) + 65536);
    d->data  = d->mtx + sizeof(pthread_mutex_t);
    if(!d->mtx)
    {
      for(int j = i; i - 1; j > -1; j--)
        free(r->pkts[j].mtx);
      free(r->ack);
      free(r->pkts);
      return LIMEADE_ERROR_MEMORY;
    }
  }

  if(pthread_create(&r->tid, NULL, *th_recv_f, ctx) != 0)
  {
    for(int i = 0; i < r->nr_pkts; i++)
      free(r->pkts[j].mtx);
    free(r->ack);
    free(r->pkts);
    return LIMEADE_ERROR_OTHER;
  }
  return LIMEADE_SUCCESS;
}

void limeade_destruct_th_recv_step_1(struct limeade_context *ctx)
{
  free(r->mtx_ack);
  free(ctx->recv);
}

void limeade_destruct_th_recv_step_2(struct limeade_context *ctx)
{
  //pthread_cancel(*ctx->recv->tid);
  pthread_mutex_lock(ctx->recv->mtx_kys);
  // TODO pthread_mutex_destroy everything
   for(int i = 0; i < r->nt_pkts; i++)
     free(r->pkts[i].mtx);
   free(r->ack);
   free(r->pkts);
}

inline void limeade_destruct_th_recv(struct limeade_context *ctx)
{
  limeade_destruct_th_recv_step_2(ctx);
  limeade_destruct_th_recv_step_1(ctx);
}

inline int limeade_init_th_csm_step_1(struct limeade_context *ctx)
{
  struct limeade_csm_data *c = malloc(sizeof(struct limeade_csm_data)
                                    + sizeof(pthread_t)
                                    + sizeof(pthread_mutex_t));
  if(!c)
    return LIMEADE_ERROR_MEMORY;

  c->hist_compr  = NULL;
  c->hist_latent = NULL;

  ctx->csm = c;
  
  c->hist_compr_sz  = LIMEADE_CSM_BENCH_BUFFER_SIZE;
  c->hist_latent_sz = LIMEADE_CSM_BENCH_BUFFER_SIZE;

  c->hist_compr_idx  = 0;
  c->hist_latent_idx = 0;

  c->freq_s = LIMEADE_CSM_FREQ_S;

  c->tid     = (pthread_t*)(c + sizeof(*c));
  c->mtx_kys = (pthread_mutex_t*)(&c->tid + sizeof(pthread_t*));

  pthread_mutex_init(c->mtx_kys);

  return LIMEADE_SUCCESS;
}

inline int limeade_init_th_csm_step_2(struct limeade_context *ctx)
{
  struct limeade_csm_data *c = ctx->csm;

  {
    uint32_t q_compr_sz  = sizeof(struct limeade_csm_compression_entry) * c->hist_compr_sz;
    uint32_t q_latent_sz = sizeof(struct limeade_csm_latency_entry)     * c->hist_latent_sz;
  
    c->hist_compr  = malloc(q_compr_sz + q_latent_sz);
    c->hist_latent = c->hist_compr + q_compr_sz;
  }
  if(!c->hist_compr)
    return LIMEADE_ERROR_MEMORY;

  if(pthread_create(&c->id, NULL, limeade_th_csm, ctx) != 0)
  {
    free(c->hist_compr);
    return LIMEADE_ERROR_OTHER;
  }
  return LIMEADE_SUCCESS;
}

void limeade_destruct_th_csm_step_1(struct limeade_context *ctx)
  free(ctx->csm);

void limeade_destruct_th_csm_step_2(struct limeade_context *ctx)
{
  struct limeade_csm_data *c = ctx->csm;
  //pthread_cancel(c->tid);
  pthread_mutex_lock(c->mtx_kys);
  // TODO await the thread's death
  free(c->hist_compr);
}

inline void limeade_destruct_th_csm(struct limeade_context *ctx)
{
  limeade_destruct_th_csm_step_2(ctx);
  limeade_destruct_th_csm_step_1(ctx);
}

inline int limeade_init_ssh_client_step_1(struct limeade_context *ctx, const char *dest)
{
  /* manages pipe/dup/execve for LIMEADE_MODE_CLIENT_SSH */

  int to_ssh[2];
  int fr_ssh[2];

  if(pipe(to_ssh) != 0)
    return LIMEADE_ERROR_OTHER;

  if(pipe(fr_ssh) != 0)
  {
    close(to_ssh[0]);
    close(to_ssh[1]);
    return LIMEADE_ERROR_OTHER;
  }

  ctx->ssh_pid = fork();

  switch(ctx->ssh_pid)
  {
    case -1:
      close(to_ssh[0]);
      close(to_ssh[1]);
      close(fr_ssh[0]);
      close(fr_ssh[1]);
      return LIMEADE_ERROR_OTHER;
    case 0:
      dup2(fr_ssh[1], STDOUT_FILENO);
      dup2(to_ssh[0], STDIN_FILENO);

      close(fr_ssh[0]);
      close(fr_ssh[1]);
      close(to_ssh[0]);
      close(to_ssh[1]);

      // ssh -s limeade user@host
      execlp("ssh", "ssh", "-s", LIMEADE_SUBSYSTEM_NAME, dest, NULL);

      exit(-1);
      
    default:
      close(fr_ssh[1]);
      close(to_ssh[0]);
      ctx->rfd = fr_ssh[0];
      ctx->sfd = to_ssh[1];
      return LIMEADE_SUCCESS;
  }
}

inline int limeade_init_ssh_client_step_2(struct limeade_context *ctx)
{
  // TODO
}

int limeade_destruct_ssh_child_step_1(struct limeade_context *ctx)
{
  kill(ctx->ssh_pid, SIGKILL);
  free(ctx->dest);
  close(ctx->rfd);
  close(ctx->sfd);
}

inline int limeade_init_eth_client_step_1(struct limeade_context *ctx,
                                          const char *dest, const int port)
{
  ctx->sfd = socket(AF_INET, SOCK_DGRAM, 0);
  ctx->rfd = ctx->sfd;

  if(ctx->sfd < 0)
    return LIMEADE_ERROR_NETWORKING;

  memset(&ctx->saddr, 0, sizeof(ctx->saddr));

  ctx->saddr.sin_family      = AF_INET;
  ctx->saddr.sin_port        = port;
  ctx->saddr.sin_addr.s_addr = inet_addr(dest);
  ctx->saddr_len = sizeof(ctx->saddr);

  if(!ctx->saddr.sin_addr.s_addr)
    return LIMEADE_ERROR_GARBAGE;

  return LIMEADE_SUCCESS;
}

inline int limeade_init_eth_client_step_2 (struct limeade_context *ctx)
{
  ctx->saddr.sin_port = ctx->port;
  // TODO bind/connect
  return LIMEADE_SUCCESS;
}

void limeade_destruct_eth_client_step_1(struct limeade_context *ctx)
{
  // TODO
}

void limeade_destruct_eth_client_step_2(struct limeade_context *ctx)
{
  // TODO
}

inline void limeade_destruct_eth_client(struct limeade_context *ctx)
{
  limeade_destruct_eth_client_step_2(ctx);
  limeade_destruct_eth_client_step_1(ctx);
}

inline int limeade_init_eth_host_step_1(struct limeade_context *ctx,
                                        const char *dest, const int port)
{
  // TODO
}

inline int limeade_init_start_eth_host_step_2(struct limeade_context *ctx,
                                              const char *dest, const int port)
{
  // TODO
}

void limeade_destruct_eth_host_step_1(struct limeade_context *ctx)
{
  // TODO
}

void limeade_destruct_eth_host_step_2(struct limeade_context *ctx)
{
  // TODO
}

inline void limeade_destruct_eth_host(struct limeade_context *ctx)
{
  limeade_destruct_eth_host_step_2(ctx);
  limeade_destruct_eth_host_step_1(ctx);
}

inline int limeade_init_libssh_client_step_1(struct limeade_context *ctx)
{
  // TODO
}

inline int limeade_init_libssh_client_step_2(struct limeade_context *ctx)
{
  // TODO
}

void limeade_destruct_libssh_client_step_1(struct limeade_context *ctx)
{
  // TODO
}

void limeade_destruct_libssh_client_step_2(struct limeade_context *ctx)
{
  // TODO
}

inline void limeade_destruct_libssh_client(struct limeade_context *ctx)
{
  limeade_destruct_libssh_client_step_2(ctx);
  limeade_destruct_libssh_client_step_1(ctx);
}

#define ERR_LBL(r, label) if(r != LIMEADE_SUCCESS) goto label
int limeade_init(struct limeade_context *ctx, uint8_t flags, ...)
{
  register int r;
  va_list arg;
  va_start(arg, flags);

  ctx->mode       = flags & 0b00001111;
  ctx->compr_mode = flags & 0b11110000;

  uint8_t allow_compr = 1;

  switch(ctx->mode)
  {
    case LIMEADE_MODE_HOST_SSH:
    {
      // this program was started by sshd, and stdin & stdout are already
      // pipes to the sshd process
      ctx->rfd = STDIN_FILENO;
      ctx->sfd = STDOUT_FILENO;
      allow_compr = 0;
    }

    case LIMEADE_MODE_CLIENT_SSH:
    {
      const char *dest = va_arg(arg, const char*);
      
      ctx->dest = malloc(++strlen(dest));
      if(!ctx->dest)
        return LIMEADE_ERROR_MEMORY;
      strcpy(ctx->dest, dest);
      
      r = limeade_init_ssh_client_step_1(ctx, dest);
      ERR_LBL(x, y); // FIXME
    }

    case LIMEADE_MODE_CLIENT_LIBSSH:
    {
#ifdef LIMEADE_HAS_SSH
      // TODO
#else
      return LIMEADE_ERROR_NOT_SUPPORTED;
#endif
    } 
    
    case LIMEADE_MODE_HOST_ETH:
    {
      const char *dest = va_arg(arg, const char*);

      ctx->dest = malloc(++strlen(dest));
      ctx->port = va_arg(arg, int);

      if(!ctx->dest)
        return LIMEADE_ERROR_MEMORY;
      strcpy(ctx->dest, dest);

      r = limeade_init_host_eth_step_1(ctx, dest, port);
      ERR_LBL(x, y); // TODO FIXME
    }

    case LIMEADE_MODE_CLIENT_ETH:
    {
      const char *dest = va_arg(arg, const char*);

      ctx->dest = malloc(++strlen(dest));
      ctx->port = va_arg(arg, int);

      if(!ctx->dest)
        return LIMEADE_ERROR_MEMORY;
      strcpy(ctx->dest, dest);

      r = limeade_init_client_eth_step_1(ctx, dest, port);
      ERR_LBL(x, y); // FIXME
    }

    default:
      return LIMEADE_ERROR_GARBAGE;
  }

  // TODO
}

int limeade_connect(struct limeade_context *ctx)
{
  // TODO
}

void limeade_release(struct limeade_context *ctx)
{
  // TODO
}

#undef ERR_LBL

