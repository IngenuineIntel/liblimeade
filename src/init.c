// init.c

#include<arpa/inet.h>
#include<errno.h>
#include<netinet/in.h>
#include<pthread.h>
#include<semaphore.h>
#include<signal.h>
#include<stdarg.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
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

static inline int limeade_init_mutexes(struct limeade_context *ctx)
{
  /* populates mtx_sfd, mtx_rfd, mtx_mode_union, mtx_compr, mtx_th_csm, &
   * mtx_pub
   */
  pthread_mutex_init(&ctx->mtx_compr_lvl,     NULL);
  pthread_mutex_init(&ctx->mtx_sfd,           NULL);
  pthread_mutex_init(&ctx->mtx_rfd,           NULL);
  pthread_mutex_init(&ctx->mtx_pub_attrs,     NULL);
  pthread_mutex_init(&ctx->mtx_mode_specific, NULL);

  return LIMEADE_SUCCESS;
}

static void limeade_destruct_mutexes(struct limeade_context *ctx)
{
  /* releases mtx_sfd, mtx_rfd, mtx_mode_union, mtx_compr, mtx_th_csm, & mtx_pub
   */
  pthread_mutex_destroy(&ctx->mtx_compr_lvl);
  pthread_mutex_destroy(&ctx->mtx_sfd);
  pthread_mutex_destroy(&ctx->mtx_rfd);
  pthread_mutex_destroy(&ctx->mtx_pub_attrs);
  pthread_mutex_destroy(&ctx->mtx_mode_specific);
}

static inline int limeade_init_th_recv_step_1(struct limeade_context *ctx)
{
  /* populates ctx->recv (step 1) */

  signal(SIGUSR1, limeade_recv_stop_signal);

  struct limeade_recv_data  *r = &ctx->recv;
  struct limeade_indiv_recv *j;

  pthread_mutex_init(&r->mtx_unread, NULL);
  pthread_mutex_init(&r->mtx_nr_read, NULL);
  pthread_mutex_init(&r->mtx_lost, NULL);
  pthread_mutex_init(&r->mtx_idx, NULL);
  sem_init(&r->sem_kys, 0, 1);
  sem_wait(&r->sem_kys);

  for(int i = 0; i < LIMEADE_NR_PKTS; i++)
  {
    j = &r->pkts[i];
    pthread_mutex_init(&j->mtx, NULL);
    j->been_read = j->ready = 0;
  }

  pthread_mutex_init(&r->ack.mtx, NULL);

  r->hz             = LIMEADE_TH_RECV_DFLT_HZ;
  r->nr_pkts_unread = 0;
  r->nr_pkts_read   = 0;
  r->nr_pkts_lost   = 0;
  r->idx_read       = 0;
  r->idx_write      = 0;
 
  return LIMEADE_SUCCESS;
}

static inline int limeade_init_th_recv_step_2(struct limeade_context *ctx)
{
  /* populates ctx->recv (step 2) */
  struct limeade_recv_data *r = &ctx->recv;
  void*(*th_recv_f)(void*);

  switch(ctx->mode)
  {
    case LIMEADE_MODE_CLIENT_SSH:
      th_recv_f = &limeade_th_recv_client_ssh;
      break;
    case LIMEADE_MODE_HOST_SSH:
      th_recv_f = &limeade_th_recv_host_ssh;
      break;
    case LIMEADE_MODE_CLIENT_LIBSSH:
      return LIMEADE_ERROR_NOT_SUPPORTED;
      break;
    case LIMEADE_MODE_CLIENT_ETH:
      th_recv_f = &limeade_th_recv_client_eth;
      break;
    case LIMEADE_MODE_HOST_ETH:
      th_recv_f = &limeade_th_recv_host_eth;
      break;
    default:
      return LIMEADE_ERROR_INVALID_CONTEXT;
  }

  if(pthread_create(&r->tid, NULL, *th_recv_f, ctx) != 0)
    return LIMEADE_ERROR_OTHER;
  return LIMEADE_SUCCESS;
}

static void limeade_destruct_th_recv_step_1(struct limeade_context *ctx)
{
  struct limeade_recv_data *r = &ctx->recv;

  for(int i = 0; i < LIMEADE_NR_PKTS; i++)
    pthread_mutex_destroy(&r->pkts[i].mtx);
  pthread_mutex_destroy(&r->mtx_unread);
  pthread_mutex_destroy(&r->mtx_nr_read);
  pthread_mutex_destroy(&r->mtx_lost);
  pthread_mutex_destroy(&r->mtx_idx);
}

static void limeade_destruct_th_recv_step_2(struct limeade_context *ctx)
{
  struct limeade_recv_data *r = &ctx->recv;
  sem_post(&r->sem_kys);
  pthread_kill(r->tid, SIGUSR1);
  pthread_join(r->tid, NULL);
  sem_destroy(&r->sem_kys);
}

#define limeade_destruct_th_recv(ctx)\
  limeade_destruct_th_recv_step_2(ctx);\
  limeade_destruct_th_recv_step_1(ctx);

static inline int limeade_init_th_csm_step_1(struct limeade_context *ctx)
{
  /* populates for the CSM thread (step 1) */
  ctx->csm.enabled = 1;
  ctx->csm.freq = LIMEADE_CSM_FREQ_MS_P;
  return LIMEADE_SUCCESS;
}

static inline int limeade_init_th_csm_step_2(struct limeade_context *ctx)
{
  /* populates for the CSM thread (step 2) */
  struct limeade_csm_data *c = &ctx->csm;

  for(int i = 0; i < LIMEADE_CSM_BENCH_BUFFER_SZ; i++)
  {
    c->hist_compr[i].ready = 0;
    c->hist_latent[i].ready = 0;
  }

  pthread_mutex_init(&c->mtx, NULL);

  if(pthread_create(&c->tid, NULL, limeade_th_csm, ctx) != 0)
    return LIMEADE_ERROR_OTHER;
  return LIMEADE_SUCCESS;
}

static inline void limeade_destruct_th_csm_step_1(struct limeade_context *ctx)
{
  /* releases data for the CSM thread (step 1) */
}

static void limeade_destruct_th_csm_step_2(struct limeade_context *ctx)
{
  /* releases data for the CSM thread (step 2) */
  pthread_mutex_destroy(&ctx->csm.mtx);
  pthread_cancel(ctx->csm.tid);
}

#define limeade_destruct_th_csm(ctx)\
  limeade_destruct_th_csm_step_2(ctx);\
  limeade_destruct_th_csm_step_1(ctx)

static int limeade_init_client_ssh_step_1(struct limeade_context *ctx, const char *dest)
{
  /* manages pipe/dup/execve for LIMEADE_MODE_CLIENT_SSH */

  if(!dest)
    return LIMEADE_ERROR_GARBAGE;

  ctx->dest = strdup(dest);

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

      // ssh -s user@host limeade
      execlp("ssh", "ssh", "-s", dest, LIMEADE_SUBSYSTEM, NULL);

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

static inline int limeade_init_client_ssh_step_2(struct limeade_context *ctx)
{
  // there's not really anything to do here except confirm that the process
  // hasn't died for some reason
  register int r = kill(ctx->ssh_pid, 0);

  if(r != 0 && errno != EPERM)
    return LIMEADE_ERROR_OTHER;
  return LIMEADE_SUCCESS;
}

static void limeade_destruct_client_ssh(struct limeade_context *ctx)
{
  kill(ctx->ssh_pid, SIGKILL);
  free(ctx->dest);
  close(ctx->rfd);
  close(ctx->sfd);
}

#define limeade_destruct_client_ssh_step_1(ctx) limeade_destruct_client_ssh(ctx)
#define limeade_destruct_client_ssh_step_2(ctx) do{}while(0);

static int limeade_init_client_eth_step_1(struct limeade_context *ctx,
                                          const char *dest, const int port)
{
  ctx->sfd = ctx->rfd = socket(AF_INET, SOCK_DGRAM, 0);

  if(ctx->sfd < 0)
    return LIMEADE_ERROR_NETWORK;

  memset(&ctx->saddr, 0, sizeof(ctx->saddr));

  ctx->dest = strdup(dest);
  ctx->port                  = port;
  //ctx->saddr.sin_port      = htons(port);
  ctx->saddr.sin_family      = AF_INET;
  ctx->saddr.sin_addr.s_addr = inet_addr(dest);

  if(!ctx->saddr.sin_addr.s_addr)
  {
    free(ctx->dest);
    close(ctx->sfd);
    return LIMEADE_ERROR_GARBAGE;
  }

  return LIMEADE_SUCCESS;
}

static inline int limeade_init_client_eth_step_2(struct limeade_context *ctx)
{
  ctx->saddr.sin_port = htons(ctx->port);

  if(connect(ctx->sfd, (struct sockaddr*)&ctx->saddr, sizeof(ctx->saddr)) < 0)
    return LIMEADE_ERROR_NETWORK;
  // layer 5 negotiation done within `limeade_connect`
  return LIMEADE_SUCCESS;
}

static void limeade_destruct_client_eth_step_1(struct limeade_context *ctx)
{
  // LIMEADE_PACKET_CLOSE not done here
  
  close(ctx->rfd);
  free(ctx->dest);
}

#define limeade_destruct_client_eth_step_2(ctx) do{}while(0);
#define limeade_destruct_client_eth(ctx) limeade_destruct_client_eth_step_1(ctx)

static inline int limeade_init_host_eth_step_1(struct limeade_context *ctx,
                                               const int port)
{
  ctx->sfd = ctx->rfd = socket(AF_INET, SOCK_DGRAM, 0);
  if(ctx->sfd < 0)
    return LIMEADE_ERROR_NETWORK;

  memset(&ctx->saddr, 0, sizeof(ctx->saddr));
  ctx->dest = NULL;
  ctx->port          = port;

  ctx->saddr_len             = sizeof(ctx->saddr);
  ctx->saddr.sin_addr.s_addr = htonl(INADDR_ANY);
  ctx->saddr.sin_family      = AF_INET;

  return LIMEADE_SUCCESS;
}

static inline int limeade_init_host_eth_step_2(struct limeade_context *ctx)
{
  ctx->saddr.sin_port = htons(ctx->port);

  if(bind(ctx->rfd, (struct sockaddr*)&ctx->saddr, sizeof(ctx->saddr)) != 0)
    return LIMEADE_ERROR_NETWORK;
  return LIMEADE_SUCCESS;
}

static inline void limeade_destruct_host_eth_step_1(struct limeade_context *ctx)
{
  close(ctx->rfd);
  free(ctx->dest);
}

#define limeade_destruct_host_eth_step_2(ctx) do{} while(0);
#define limeade_destruct_host_eth(ctx) limeade_destruct_host_eth_step_1(ctx)

#define CHECK(r) if(r != LIMEADE_SUCCESS)
int limeade_init(struct limeade_context *ctx, int flags, ...)
{
  register int r = LIMEADE_SUCCESS;
  va_list arg;
  va_start(arg, flags);
  
  ctx->mode       = (uint8_t)flags & 0b00001111;
  ctx->compr_mode = (uint8_t)flags & 0b11110000;

  ctx->dest       = NULL;
  ctx->csm.enabled = 0;

  uint8_t allow_compr = 1;
  
  switch(ctx->mode)
  {
    case LIMEADE_MODE_HOST_SSH:
      // this program was started by sshd, and stdin & stdout are already pipes
      // to te sshd process, whom the traffic is tunneled through
      ctx->rfd = STDIN_FILENO;
      ctx->sfd = STDOUT_FILENO;
      break;
    case LIMEADE_MODE_CLIENT_SSH:
      r = limeade_init_client_ssh_step_1(ctx, va_arg(arg, const char *));
      // allow_compr = 0;
      break;
    case LIMEADE_MODE_HOST_ETH:
      r = limeade_init_host_eth_step_1(ctx, va_arg(arg, int));
      break;
    case LIMEADE_MODE_CLIENT_ETH:
      r = limeade_init_client_eth_step_1(ctx, va_arg(arg, const char *), va_arg(arg, const int));
      break;
    default:
      return LIMEADE_ERROR_GARBAGE;
  }
  CHECK(r)
    return r;

  r = limeade_init_mutexes(ctx);
  CHECK(r)
    goto err_1;

  if(allow_compr)
  {
    switch(ctx->compr_mode)
    {
      case LIMEADE_MODE_NO_COMPRESSION:
        ctx->compr_lvl = 0;
        break;
      case LIMEADE_MODE_LOW_COMPRESSION:
        ctx->compr_lvl = 1;
        break;
      case LIMEADE_MODE_MED_COMPRESSION:
        ctx->compr_lvl = 3;
        break;
      case LIMEADE_MODE_HIGH_COMPRESSION:
        ctx->compr_lvl = 6;
        break;
      default:
        r = limeade_init_th_csm_step_1(ctx);

        CHECK(r)
          goto err_2;
        break;
    }
  }

  r = limeade_init_th_recv_step_1(ctx);
  CHECK(r)
    goto err_3;

  return LIMEADE_SUCCESS;

err_3:
  if(ctx->csm.enabled)
    limeade_destruct_th_csm_step_1(ctx);
err_2:
  limeade_destruct_mutexes(ctx);
err_1:
  switch(ctx->mode)
  {
    case LIMEADE_MODE_CLIENT_SSH:
      limeade_destruct_client_ssh_step_1(ctx);
      break;
    case LIMEADE_MODE_HOST_ETH:
      limeade_destruct_host_eth_step_1(ctx);
      break;
    case LIMEADE_MODE_CLIENT_ETH:
      limeade_destruct_client_eth_step_1(ctx);
      break;
  }
  return r;
}

int limeade_connect(struct limeade_context *ctx)
{
  register int r;

  switch(ctx->mode)
  {
    case LIMEADE_MODE_CLIENT_SSH:
      r = limeade_init_client_ssh_step_2(ctx);
      CHECK(r) goto err_1;
      break;

    case LIMEADE_MODE_HOST_ETH:
      r = limeade_init_host_eth_step_2(ctx);
      CHECK(r) goto err_1;
      break;

    case LIMEADE_MODE_CLIENT_ETH:
      r = limeade_init_client_eth_step_2(ctx);
      CHECK(r) goto err_1;
      break;
  }

  r = limeade_init_th_recv_step_2(ctx);
  CHECK(r) goto err_2;

  if(ctx->csm.enabled)
  {
    r = limeade_init_th_csm_step_2(ctx);
    CHECK(r) goto err_3;
  }

  return LIMEADE_SUCCESS;

err_3:
  limeade_destruct_th_recv_step_2(ctx);
err_2:
  switch(ctx->mode)
  {
    case LIMEADE_MODE_CLIENT_SSH:
      limeade_destruct_client_ssh_step_2(ctx);
      break;
    case LIMEADE_MODE_HOST_ETH:
      limeade_destruct_host_eth_step_2(ctx);
      break;
    case LIMEADE_MODE_CLIENT_ETH:
      limeade_destruct_client_eth_step_2(ctx);
      break;
  }

err_1:
  limeade_destruct_th_recv_step_1(ctx);
  if(ctx->csm.enabled)
    limeade_destruct_th_csm(ctx);
  limeade_destruct_mutexes(ctx);
  switch(ctx->mode)
  {
    case LIMEADE_MODE_CLIENT_SSH:
      limeade_destruct_client_ssh_step_1(ctx);
      break;
    case LIMEADE_MODE_CLIENT_ETH:
      limeade_destruct_client_eth_step_1(ctx);
      break;
    case LIMEADE_MODE_HOST_ETH:
      limeade_destruct_host_eth_step_1(ctx);
      break;
  }

  return r;
}

void limeade_destruct(struct limeade_context *ctx)
{
  limeade_destruct_th_recv(ctx);
  if(ctx->csm.enabled)
    limeade_destruct_th_csm(ctx);
  limeade_destruct_mutexes(ctx);
  switch(ctx->mode)
  {
    case LIMEADE_MODE_CLIENT_SSH:
      limeade_destruct_client_ssh(ctx);
      break;
    case LIMEADE_MODE_CLIENT_ETH:
      limeade_destruct_client_eth(ctx);
      break;
    case LIMEADE_MODE_HOST_ETH:
      limeade_destruct_host_eth(ctx);
      break;
  }
}

