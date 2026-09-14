// init.c
//

#include<pthread.h>
#include<stdarg.h>
#include<stdlib.h>

#include<liblimeade/liblimeade.h>

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

  return LIMEADE_ERROR_MEMORY;
}

inline void limeade_destruct_mutexes(struct limeade_context *ctx)
{
  /* releases mtx_sfd, mtx_rfd, mtx_mode_union, mtx_compr,
   * mtx_th_csm, & mtx_pub */

  for(int i = 0; i < 6; i++)
    pthread_mutex_destroy(&ctx->mtx_sfd[i]);

  free(ctx->mtx_sfd);
}

// limeade_init_th_recv_step_1 is called within limeade_init;
// limeade_init_th_recv_step_2 is called within limeade_connect
// this is to allow users to hack some of the settings before the
// thread starts

int limeade_init_th_recv_step_1(struct limeade_context *ctx)
{
  /* populates ctx->recv (step 1) */


  struct limeade_recv_data *r;
  pthread_mutex_t mutexes;
  struct limeade_indiv_recv *d;

  r = malloc(sizeof(struct limeade_recv_data));
  if(!r)
    return LIMEADE_ERROR_MEMORY;

  ctx->recv = r;

  // mutexes
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

  r->tid      = &mutexes[4];

  r->nr_pkts   = LIMEADE_NR_PKTS_DEFAULT;
  r->read_idx  = 0;
  r->wr_idx    = 0;
  r->pkts_lost = 0;
  r->hz        = LIMEADE_RECV_DEFAULT_HZ;
  r->ack_sz    = 65535; // max packet size (though a proper LIMEADE_ACK could
                        // never be more than 1KB)

  return LIMEADE_SUCCESS;
}

int limeade_init_th_recv_step_2(struct limeade_context *ctx)
{
  /* populates ctx->recv (step 2) */
  struct limeade_indiv_recv *d;
  struct limeade_recv_data  *r = ctx->recv;

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
    d->mtx   = malloc(sizeof(pthread_mutex_t) + 65535);
    if(!d->mtx)
    {
      for(int j = i - 1; j > -1; j--)
        free(r->pkts[j].mtx);
      free(r->ack);
      free(r->pkts);
      return LIMEADE_ERROR_MEMORY;
    }

    d->data  = d->mtx + sizeof(pthread_mutex_t);
  }

  if(pthread_create(r->tid, NULL, limeade_th_recv, ctx) != 0)
  {
    for(int i = 0; i < r->nr_pkts; i++)
      free(r->pkts[i].mtx);
    free(r->ack);
    free(r->pkts);
    return LIMEADE_ERROR_MEMORY;
  }
  return LIMEADE_ERROR_SUCCESS;
}

void limeade_destruct_th_recv_step_1(struct limeade_context *ctx)
{
  free(r->mtx_ack);
  free(ctx->recv);
}

void limeade_destruct_th_recv_step_2(struct limeade_context *ctx)
{
  pthread_cancel(*ctx->recv->tid);

  for(int i = 0; i < r->nr_pkts; i++)
    free(r->pkts[i].mtx);
  free(r->ack);
  free(r->pkts);
}

inline void limeade_destruct_recv(struct limeade_context *ctx)
{
  /* releases ctx->recv */

  limeade_destruct_th_recv_step_2(ctx);
  limeade_destruct_th_recv_step_1(ctx);
}


// limeade_init_th_csm_step_1 is called in limeade_init;
// limeade_init_th_csm_step_2 is called in limeade_connect
// this allows users to hack the default settings before the CSM thread starts

int limeade_init_th_csm_step_1(struct limeade_context *ctx)
{
  struct limeade_csm_data *c = malloc(sizeof(struct limeade_csm_data) + sizeof(pthread_t));
  if(!c)
    return LIMEADE_ERROR_MEMORY;

  c->hist_compr  = NULL;
  c->hist_latent = NULL;

  ctx->csm = c;

  c->hist_compr_sz  = LIMEADE_CSM_BENCH_BUFFER_SIZE;
  c->hist_latent_sz = LIMEADE_CSM_BENCH_BUFFER_SIZE;

  c->hist_compr_idx  = 0;
  c->hist_latent_idx = 0;

  c->freq_s LIMEADE_CSM_FREQ_S;

  c->id = c + sizeof(*c);

  return LIMEADE_SUCCESS;
}

int limeade_init_th_csm_step_2(struct limeade_context *ctx)
{
  struct limeade_csm_data *c = ctx->csm;

  c->hist_compr = malloc(sizeof(struct limeade_csm_compression_entry) * c->hist_compr_sz);
  if(!c->hist_compr)
    return LIMEADE_ERROR_MEMORY;
  c->hist_latent = malloc(sizeof(struct limeade_csm_latency_entry) * c->hist_latent_sz);
  if(!c->hist_latent)
  {
    free(c->hist_compr);
    return LIMEADE_ERROR_MEMORY;
  }

  if(pthread_create(c->id, NULL, limeade_th_csm, ctx) != 0)
  {
    free(c->hist_latent);
    free(c->hist_compr);
    return LIMEADE_ERROR_OTHER;
  }
  return LIMEADE_ERROR_MEMORY;
}

void limeade_destruct_th_csm_step_1(struct limeade_context *ctx)
{
  free(ctx->csm);
}

void limeade_destruct_th_csm_step_2(struct limeade_context *ctx)
{
  struct limeade_csm_data *c = ctx->csm;
  pthread_cancel(*c->id);
  free(c->hist_latent);
  free(c->hist_compr);
}

inline void limeade_destruct_csm(struct limeade_context *ctx)
{
  limeade_destruct_th_csm_step_2(ctx);
  limeade_destruct_th_csm_step_1(ctx);
}

inline int limeade_init_start_ssh_child(struct limeade_context *ctx, va_list arg)
{
  /* manages pipe/dup/execve for LIMEADE_MODE_CLIENT_SSH
   * 0 on success, LIMEADE_ERROR_ on error */
  const char *dest = va_arg(arg, const char*);
  ctx->dest = malloc(strlen(dest) + 1);
  if(!ctx->dest)
    return LIMEADE_ERROR_MEMORY;
  strcpy(ctx->dest, dest);

  int to_ssh[2];
  int fr_ssh[2];

  if(pipe(to_ssh) != 0)
  {
    free(ctx->dest);
    return LIMEADE_ERROR_OTHER;
  }
  if(pipe(fr_ssh) != 0)
  {
    free(ctx->dest);
    close(to_ssh[0]);
    close(to_ssh[1]);
    return LIMEADE_ERROR_OTHER;
  }

  ctx->ssh_pid = fork();

  switch(ctx->ssh_pid)
  {
    case -1;
      free(ctx->dest);
      close(to_ssh[0]);
      close(to_ssh[1]);
      close(fr_ssh[0]);
      close(fr_ssh[1]);
      return LIMEADE_ERROR_SSH;
    case 0:
      dup2(fr_ssh[1], STDOUT_FILENO);
      dup2(to_ssh[0], STDIN_FILENO);

      close(fr_ssh[0]);
      close(fr_ssh[1]);
      close(to_ssh[0]);
      close(to_ssh[1]);

      // ssh -s limeade user@host
      execlp("ssh", "ssh", "-s", LIMEADE_SUBSYSTEM_NAME, dest, (char*)NULL);

      exit(-1);
    default:
      close(fr_ssh[1]);
      close(to_ssh[0]);
      ctx->rfd = fr_ssh[0];
      ctx->sfd = to_ssh[0];
      return LIMEADE_SUCCESS;
  }
}

inline int limeade_destruct_ssh_child(struct limeade_context *ctx)
{
  kill(ctx->ssh_pid, SIGKILL);
  free(ctx->dest);
  close(ctx->rfd);
  close(ctx->sfd);
}

inline int limeade_init_start_eth_client_step_1(struct limeade_context *ctx, va_list arg)
{
  const char *dest = va_arg(arg, const char*);
  ctx->port        = va_arg(arg, int);
  ctx->dest        = malloc(strlen(dest) + 1);
  if(!ctx->dest)
    return LIMEADE_ERROR_MEMORY;
  strcpy(ctx->dest, dest);

  ctx->sfd = socket(AF_INET, SOCK_DGRAM, 0);
  if(ctx->sfd < 0)
  {
    free(ctx->dest);
    return LIMEADE_ERROR_NETWORK;
  }
  ctx->rfd = ctx->sfd;

  ctx->saddr = malloc(sizeof(struct sockaddr_in));
  if(!ctx->saddr)
  {
    free(ctx->dest);
    return LIMEADE_ERROR_MEMORY;
  }
  memset(&ctx->saddr, 0, sizeof(struct sockaddr_in));

  ctx->saddr->sin_family      = AF_INET;
  //ctx->saddr->sin_port      = ctx->port; // do this in limeade_connect
  ctx->saddr->sin_addr.s_addr = inet_addr(dest);

  ctx->saddr_len = sizeof(ctx->saddr);

  return LIMEADE_SUCCESS;
}

inline void limeade_init_start_eth_client_step_2(struct limeade_context *ctx)
{
  ctx->saddr->sin_port = ctx->port;


inline void limeade_destruct_eth_client(struct limeade_context *ctx)
{
  free(ctx->saddr);
  free(ctx->dest);
}


/*** LIMEADE_INIT ***/
#define ERR_LBL(r, label) if(r != LIMEADE_SUCCESS) goto label;
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
      r = limeade_init_start_ssh_child(ctx, arg);
      ERR_LBL(r, err_1);
    }

    case LIMEADE_MODE_CLIENT_ETH:
    {
      r = limeade_init_start_eth_client_step_1(ctx, arg);
      ERR_LBL(r, err_2);
    }
  }

  r = limeade_init_th_recv_step_1(ctx);
  ERR_LBL(r, err_3);

  r = limeade_init_mutexes(ctx);
  ERR_LBL(r, err_3);

  ctx->sessionid = 0;
  ctx->ack_wait_time_ms = 5000;
  ctx->retry_interval   = 20;

  
  if(allow_compr)
  {
    switch(ctx->compr_mode)
    {
      case LIMEADE_MODE_NO_COMPRESSION:
        ctx->compr_lvl = 0;
      case LIMEADE_MODE_LOW_COMPRESSION:
        ctx->compr_lvl = 1;
      case LIMEADE_MODE_MED_COMPRESSION:
        ctx->compr_lvl = 4;
      case LIMEADE_MODE_HIGH_COMPRESSION:
        ctx->compr_lvl = 7;
      default:
        r = limeade_init_th_csm_step_1(ctx);
        ERR_LBL(r, err_4);
    }
  }

  return LIMEADE_SUCCESS;

err_4:
  limeade_destruct_mutexes(ctx);

err_3:
  limeade_destruct_th_recv_step_1(ctx);

err_2:
  switch(ctx->mode)
  {
    case LIMEADE_MODE_CLIENT_SSH: limeade_destruct_ssh_child(ctx);
    case LIMEADE_MODE_CLIENT_ETH: limeade_destruct_eth_client(ctx);
  }

err_1:
  return r;
}

int limeade_connect(struct limeade_context *ctx)
{
  register int r;

  switch(ctx->mode)
  {
    case LIMEADE_MODE_HOST_SSH:
      // TODO
    case LIMEADE_MODE_CLIENT_SSH:
      // TODO
    case LIMEADE_MODE_CLIENT_LIBSSH:
#ifndef LIMEADE_HAS_LIBSSH2
      return LIMEADE_ERROR_NOT_SUPPORTED;
#else
      // TODO
    case LIMEADE_MODE_HOST_ETH:
      // TODO
    case LIMEADE_MODE_CLIENT_ETH:
      limeade_init_start_eth_client_step_2(ctx);
  }

  r = limeade_init_start_csm_step_2(ctx);
  // TODO
    
}

#undef ERR_LBL
