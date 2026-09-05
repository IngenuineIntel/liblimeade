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
    return -1;

  for(int i = 0; i < 6; i++)
    pthread_mutex_init(mutexes[i]);

  ctx->mtx_sfd        = mutexes[0];
  ctx->mtx_rfd        = mutexes[1];
  ctx->mtx_mode_union = mutexes[2];
  ctx->mtx_compr      = mutexes[3];
  ctx->mtx_th_csm     = mutexes[4];
  ctx->mtx_pub        = mutexes[5];

  return 0;
}

inline void limeade_destruct_mutexes(struct limeade_context *ctx)
{
  /* releases mtx_sfd, mtx_rfd, mtx_mode_union, mtx_compr,
   * mtx_th_csm, & mtx_pub */

  for(int i = 0; i < 6; i++)
    pthread_mutex_destroy(ctx->mtx_sfd[i]);

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
    return -1;

  ctx->recv = r;

  // mutexes
  mutexes = malloc(sizeof(pthread_mutex_t) * 4 + sizeof(pthread_t));
  if(!mutexes)
  {
    free(r);
    return -1;
  }

  for(int i = 0; i < 4; i++)
    pthread_mutex_init(mutexes[i]);

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

  return 0;
}

int limeade_init_th_recv_step_2(struct limeade_context *ctx)
{
  /* 0 on success, -1 for memory error -2 for pthread error */
  struct limeade_recv_data *r = ctx->recv;

  r->pkts = malloc(sizeof(struct limeade_indiv_recv) * r->nr_pkts);
  if(!r->pkts)
    return -1;

  r->ack = malloc(r->ack_sz);
  if(!r->ack)
  {
    free(r->pkts);
    return -1;
  }

  for(int i = 0; i < r->nr_pkts; i++)
  {
    d = r->pkts[i];

    d->sz    = 0;
    d->flags = 0;
    d->mtx   = malloc(sizeof(pthread_mutex_t) + 65535);
    if(!d->mtx)
    {
      for(int j = i - 1; j > -1; j--)
        free(r->pkts[j].mtx);
      free(r->ack);
      free(r->pkts);
      return -1;
    }

    d->data  = d->mtx + sizeof(pthread_mutex_t);
  }

  if(pthread_create(r->tid, NULL, limeade_th_recv, ctx) != 0)
  {
    for(int i = 0; i < r->nr_pkts; i++)
      free(r->pkts[i].mtx);
    free(r->ack);
    free(r->pkts);
    return -2;
  }
  return 0;
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
    return -1;

  c->hist_compr  = NULL;
  c->hist_latent = NULL;

  ctx->csm = c;

  c->hist_compr_sz  = LIMEADE_CSM_BENCH_BUFFER_SIZE;
  c->hist_latent_sz = LIMEADE_CSM_BENCH_BUFFER_SIZE;

  c->hist_compr_idx  = 0;
  c->hist_latent_idx = 0;

  c->freq_s LIMEADE_CSM_FREQ_S;

  c->id = c + sizeof(*c);

  return 0;
}

int limeade_init_th_csm_step_2(struct limeade_context *ctx)
{
  struct limeade_csm_data *c = ctx->csm;

  c->hist_compr = malloc(sizeof(struct limeade_csm_compression_entry) * c->hist_compr_sz);
  if(!c->hist_compr)
    return -1;
  c->hist_latent = malloc(sizeof(struct limeade_csm_latency_entry) * c->hist_latent_sz);
  if(!c->hist_latent)
  {
    free(c->hist_compr);
    return -1;
  }

  if(pthread_create(c->id, NULL, limeade_th_csm, ctx) != 0)
  {
    free(c->hist_latent);
    free(c->hist_compr);
    return -2;
  }
  return 0;
}

inline void limeade_destruct_th_csm_step_1(struct limeade_context *ctx)
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

