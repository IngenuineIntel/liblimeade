// init.c

#include<arpa/inet.h>
#include<errno.h>
#include<netinet/in.h>
#include<pthread.h>
#include<signal.h>
#include<stdarg.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<unistd.h>

#include<liblimeade/liblimeade-internal.h>

// limeade_init_*_step_1
//   executed within `limeade_init`
// limeade_init_*_step_2
//   executed within `limeade_connect`
// limeade_destruct_*_step_*
//   executed in the event of failure in `limeade_init` & `limeade_connect`
// limeade_destruct_*
//   executed in `limeade_release`

inline int limeade_init_mutexes(struct limeade_context *ctx)
{
  /* populates mtx_sfd, mtx_rfd, mtx_mode_union, mtx_compr, mtx_th_csm, &
   * mtx_pub
   */

  pthread_mutex_t *mutexes = malloc(sizeof(pthread_mutex_t) * 6);
  if(!mutexes)
    return LIMEADE_ERROR_MEMORY;

  for(int i = 0; i < 6; i++)
    pthread_mutex_init(&mutexes[i], NULL);
  
  ctx->mtx_sfd        = &mutexes[0];
  ctx->mtx_rfd        = &mutexes[1];
  ctx->mtx_mode_union = &mutexes[2];
  ctx->mtx_compr      = &mutexes[3];
  ctx->mtx_th_csm     = &mutexes[4];
  ctx->mtx_pub        = &mutexes[5];

  return LIMEADE_SUCCESS;
}

void limeade_destruct_mutexes(struct limeade_context *ctx)
{
  /* releases mtx_sfd, mtx_rfd, mtx_mode_union, mtx_compr, mtx_th_csm, & mtx_pub
   */
  for(int i = 0; i < 6; i++)
    pthread_mutex_destroy(&ctx->mtx_sfd[i]);

  free(ctx->mtx_sfd);
}

inline int limeade_init_th_recv_step_1(struct limeade_context *ctx)
{
  /* populates ctx->recv (step 1) */

  struct limeade_recv_data *r;
  pthread_mutex_t *mutexes;
  struct limeade_indiv_recv *d;

  r = &ctx->recv;

  // mutexes
  // this allocation is 4 mutexes, then a pthread_t at the endj
  mutexes = malloc(sizeof(pthread_mutex_t) * 4 + sizeof(pthread_t));
  if(!mutexes)
    return LIMEADE_ERROR_MEMORY;

  for(int i = 0; i < 4; i++)
    pthread_mutex_init(&mutexes[i], NULL);

  r->mtx_ack  = &mutexes[0];
  r->mtx_idx  = &mutexes[1];
  r->mtx_kys  = &mutexes[2];
  r->mtx_lost = &mutexes[3];
  r->tid      = (pthread_t*)&mutexes[4];

  r->nr_pkts = LIMEADE_NR_PKTS_DEFAULT;
  r->read_idx = 0;
  r->wr_idx   = 0;
  r->pkts_lost = 0;
  r->hz         = LIMEADE_RECV_DEFAULT_HZ;
  r->ack_sz     = 1 << 16;

  return LIMEADE_SUCCESS;
}

inline int limeade_init_th_recv_step_2(struct limeade_context *ctx)
{
  /* populates ctx->recv (step 2) */

  struct limeade_indiv_recv *d;
  struct limeade_recv_data *r = &ctx->recv;
  
  void*(*th_recv_f)(void*);

  switch(ctx->mode)
  {
    case LIMEADE_MODE_CLIENT_SSH:
      th_recv_f = &limeade_th_recv_client_ssh;
    case LIMEADE_MODE_HOST_SSH:
      th_recv_f = &limeade_th_recv_host_ssh;
    case LIMEADE_MODE_CLIENT_LIBSSH:
      return LIMEADE_ERROR_NOT_SUPPORTED;
    case LIMEADE_MODE_CLIENT_ETH:
      th_recv_f = &limeade_th_recv_client_eth;
    case LIMEADE_MODE_HOST_ETH:
      th_recv_f = &limeade_th_recv_host_eth;
    default:
      return LIMEADE_ERROR_INVALID_CONTEXT;
  }

  // buffer for r->nr_pkts & r->ack
  r->pkts = malloc(sizeof(*d) * r->nr_pkts + r->ack_sz);
  if(!r->pkts)
    return LIMEADE_ERROR_MEMORY;

  r->ack = r->pkts + (sizeof(*d) * r->nr_pkts);

  for(int i = 0; i < r->nr_pkts; i++)
  {
    d = &r->pkts[i];

    d->sz = 0;
    d->flags = 0;
    d->mtx   = malloc(sizeof(pthread_mutex_t) + (1 << 16));
    d->data  = d->mtx + sizeof(pthread_mutex_t);
    if(!d->mtx)
    {
      for(int j = i - 1; j > -1; j++)
        free(r->pkts[j].mtx);
      free(r->pkts);
      return LIMEADE_ERROR_MEMORY;
    }
  }

  if(pthread_create(r->tid, NULL, *th_recv_f, ctx) != 0)
  {
    for(int i = 0; i < r->nr_pkts; i++)
      free(r->pkts[i].mtx);
    free(r->pkts);
    return LIMEADE_ERROR_OTHER;
  }
  return LIMEADE_SUCCESS;
}

void limeade_destruct_th_recv_step_1(struct limeade_context *ctx)
{
  free(ctx->recv.mtx_ack);
}

void limeade_destruct_th_recv_step_2(struct limeade_context *ctx)
{
  struct limeade_recv_data *r = &ctx->recv;
  //pthread_cancel(r->tid);
  pthread_mutex_lock(r->mtx_kys);
  pthread_join(*(pthread_t*)r->tid, NULL);

  pthread_mutex_t *m;
  for(int i = 0; i < r->nr_pkts; i++)
  {
    m = r->pkts[i].mtx;
    pthread_mutex_destroy(m);
    free(m);
  }
  free(r->pkts);
}

#define limeade_destruct_th_recv(ctx)\
  limeade_destruct_th_recv_step_2(ctx);\
  limeade_destruct_th_recv_step_1(ctx);

inline int limeade_init_th_csm_step_1(struct limeade_context *ctx)
{
  /* populates for the CSM thread (step 1) */
  struct limeade_csm_data *c = malloc(sizeof(*c)
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

  c->tid     = (pthread_t*)((void*)c + sizeof(*c));
  c->mtx_kys = (pthread_mutex_t*)(c->tid + sizeof(pthread_t*));

  pthread_mutex_init(c->mtx_kys, NULL);

  return LIMEADE_SUCCESS;
}

inline int limeade_init_th_csm_step_2(struct limeade_context *ctx)
{
  /* populates for the CSM thread (step 2) */
  uint32_t q_compr_sz, q_latent_sz;
  struct limeade_csm_data *c;

  c = ctx->csm;
  
  q_compr_sz  = sizeof(struct limeade_csm_compression_entry) * c->hist_compr_sz;
  q_latent_sz = sizeof(struct limeade_csm_latency_entry)     * c->hist_latent_sz;

  c->hist_compr  = malloc(q_compr_sz + q_latent_sz);
  c->hist_latent = (struct limeade_csm_latency_entry*)(c->hist_compr + q_compr_sz);

  if(!c->hist_compr)
    return LIMEADE_ERROR_MEMORY;

  if(pthread_create(c->tid, NULL, limeade_th_csm, ctx) != 0)
  {
    free(c->hist_compr);
    return LIMEADE_ERROR_OTHER;
  }
  return LIMEADE_SUCCESS;
}

void limeade_destruct_th_csm_step_1(struct limeade_context *ctx)
{
  /* releases data for the CSM thread (step 1) */
  free(ctx->csm);
}

void limeade_destruct_th_csm_step_2(struct limeade_context *ctx)
{
  /* releases data for the CSM thread (step 2) */
  struct limeade_csm_data *c = ctx->csm;
  pthread_mutex_lock(c->mtx_kys);
  pthread_join(*(pthread_t*)c->tid, NULL);

  free(c->hist_compr);
}

inline void limeade_destruct_th_csm(struct limeade_context *ctx)
{
  limeade_destruct_th_csm_step_2(ctx);
  limeade_destruct_th_csm_step_1(ctx);
}

inline int limeade_init_client_ssh_step_1(struct limeade_context *ctx, const char *dest)
{
  /* manages pipe/dup/execve for LIMEADE_MODE_CLIENT_SSH */

  int s = strlen(dest) + 1;
  ctx->dest = malloc(s);
  if(!ctx->dest)
    return LIMEADE_ERROR_MEMORY;

  strncpy(ctx->dest, dest, s);

  int to_ssh[2];
  int fr_ssh[2];

  if(pipe(to_ssh) != 0)
    goto err_1;

  if(pipe(fr_ssh) != 0)
    goto err_2;

  ctx->ssh_pid = fork();

  switch(ctx->ssh_pid)
  {
    case -1:
      goto err_3;
    case 0:
      dup2(fr_ssh[1], STDOUT_FILENO);
      dup2(to_ssh[0], STDIN_FILENO);

      close(fr_ssh[0]);
      close(fr_ssh[1]);
      close(to_ssh[0]);
      close(to_ssh[1]);

      // ssh -s limeade user@host
      execlp("ssh", "ssh", "-s", LIMEADE_SUBSYSTEM, dest, NULL);

      exit(-1);

    default:
      close(fr_ssh[1]);
      close(to_ssh[0]);
      ctx->rfd = fr_ssh[0];
      ctx->sfd = to_ssh[1];
      return LIMEADE_SUCCESS;
  }

err_3:
  close(fr_ssh[0]);
  close(fr_ssh[1]);
err_2:
  close(to_ssh[0]);
  close(to_ssh[1]);
err_1:
  free(ctx->dest);
  return LIMEADE_ERROR_OTHER;
}

inline int limeade_init_client_ssh_step_2(struct limeade_context *ctx)
{
  // there's not really anything to do here except confirm that the process
  // hasn't died for some reason
  register int r = kill(ctx->ssh_pid, 0);

  if(r != 0 && errno != EPERM)
    return LIMEADE_ERROR_OTHER;
  return LIMEADE_SUCCESS;
}

void limeade_destruct_client_ssh(struct limeade_context *ctx)
{
  kill(ctx->ssh_pid, SIGKILL);
  free(ctx->dest);
  close(ctx->rfd);
  close(ctx->sfd);
}

inline int limeade_init_client_eth_step_1(struct limeade_context *ctx,
                                          const char *dest, const int port)
{
  int s;

  ctx->sfd = ctx->rfd = socket(AF_INET, SOCK_DGRAM, 0);

  if(ctx->sfd < 0)
    return LIMEADE_ERROR_NETWORK;

  memset(&ctx->saddr, 0, sizeof(ctx->saddr));

  s = strlen(dest) + 1;
  ctx->dest = malloc(s);
  if(!ctx->dest)
  {
    close(ctx->sfd);
    return LIMEADE_ERROR_MEMORY;
  }
  strncpy(ctx->dest, dest, s);


  ctx->port                  = port;
  //ctx->saddr.sin_port      = htons(port);
  ctx->saddr.sin_family      = AF_INET;
  ctx->saddr.sin_addr.s_addr = inet_addr(dest);
  ctx->saddr_len             = sizeof(ctx->saddr);

  if(!ctx->saddr.sin_addr.s_addr)
  {
    free(ctx->dest);
    close(ctx->sfd);
    return LIMEADE_ERROR_GARBAGE;
  }

  return LIMEADE_SUCCESS;
}

inline int limeade_init_client_eth_step_2(struct limeade_context *ctx)
{
  ctx->saddr.sin_port = htons(ctx->port);

  if(connect(ctx->sfd, (struct sockaddr*)&ctx->saddr, ctx->saddr_len) < 0)
    return LIMEADE_ERROR_NETWORK;
  // layer 5 negotiation done within `limeade_connect`
  return LIMEADE_SUCCESS;
}

void limeade_destruct_client_eth_step_1(struct limeade_context *ctx)
{
  // LIMEADE_PACKET_CLOSE not done here
  
  close(ctx->rfd);
  free(ctx->dest);
}

#define limeade_destruct_eth_client(ctx) limeade_destruct_eth_client_step_1(ctx)

inline int limeade_init_host_eth_step_1(struct limeade_context *ctx,
                                        const char *dest, const int port)
{
  int s;

  ctx->sfd = ctx->rfd = socket(AF_INET, SOCK_DGRAM, 0);

  memset(&ctx->saddr, 0, sizeof(ctx->saddr));

  s = strlen(dest) + 1;
  ctx->dest = malloc(s);
  if(!ctx->dest)
  {
    close(ctx->sfd);
    return LIMEADE_ERROR_MEMORY;
  }
  strncpy(ctx->dest, dest, s);
  ctx->port          = port;

  //ctx->saddr.sin_port      = htons(ctx->port);
  ctx->saddr.sin_addr.s_addr = htonl(INADDR_ANY);
  ctx->saddr.sin_family      = AF_INET;

  return LIMEADE_SUCCESS;
}

inline int limeade_init_host_eth_step_2(struct limeade_context *ctx)
{
  ctx->saddr.sin_port = htons(ctx->port);

  if(bind(ctx->rfd, (struct sockaddr*)&ctx->saddr, ctx->saddr_len) != 0)
    return LIMEADE_ERROR_NETWORK;
  return LIMEADE_SUCCESS;
}

void limeade_destruct_host_eth_step_1(struct limeade_context *ctx)
{
  close(ctx->rfd);
  free(ctx->dest);
}

#define limeade_destruct_eth_host(ctx) limeade_destruct_eth_host_step_1(ctx)

inline int limeade_init_client_libssh_step_1(struct limeade_context *ctx,
                                             const char *dest)
{
  return LIMEADE_ERROR_NOT_SUPPORTED;
}

inline int limeade_init_client_libssh_step_2(struct limeade_context *ctx)
{
  return LIMEADE_ERROR_NOT_SUPPORTED;
}

void limeade_destruct_client_libssh_step_1(struct limeade_context *ctx)
{
  return; // LIMEADE_ERROR_NOT_SUPPORTED
}

void limeade_destruct_client_libssh_step_2(struct limeade_context *ctx)
{
  return; // LIMEADE_ERROR_NOT_SUPPORTED
}

#define limeade_destruct_client_libssh(ctx)\
  limeade_destruct_client_libssh_step_2(ctx);\
  limeade_destruct_client_libssh_step_1(ctx);


int limeade_init(struct limeade_context *ctx, int flags, ...)
{
  register int r;
  va_list arg;
  va_start(arg, flags);
  
  ctx->mode       = (uint8_t)flags & 0b00001111;
  ctx->compr_mode = (uint8_t)flags & 0b11110000;

  uint8_t allow_compr = 1;
  
  switch(ctx->mode)
  {
    case LIMEADE_MODE_HOST_SSH:
      // this program was started by sshd, and stdin & stdout are already pipes
      // to te sshd process, whom the traffic is tunneled through
      ctx->rfd = STDIN_FILENO;
      ctx->sfd = STDOUT_FILENO;
      allow_compr = 0;
    case LIMEADE_MODE_CLIENT_SSH:
      r = limeade_init_client_ssh_step_1(ctx, va_arg(arg, const char *));
      allow_compr = 0;
    case LIMEADE_MODE_CLIENT_LIBSSH:
      r = limeade_init_client_libssh_step_1(ctx, va_arg(arg, const char *));
      allow_compr = 0;
    case LIMEADE_MODE_HOST_ETH:
      r = limeade_init_host_eth_step_1(ctx, va_arg(arg, const char *), va_arg(arg, const int));
    case LIMEADE_MODE_CLIENT_ETH:
      r = limeade_init_client_eth_step_1(ctx, va_arg(arg, const char *), va_arg(arg, const int));
    default:
      return LIMEADE_ERROR_GARBAGE;
  }

  if(r != LIMEADE_SUCCESS)
    return r;

  // TODO
}






















