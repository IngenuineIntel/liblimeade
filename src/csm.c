// csm.c
//
// AGPL

#include<liblimeade/liblimeade-internal.h>

void *limeade_csm(void *arg)
{
  // TODO
}

void limeade_csm_add_compr_entry(struct limeade_context *ctx,
                                 struct limeade_csm_compression_entry *entry)
{
  struct limeade_csm_data *csm = ctx->csm;

  if(csm == NULL)
  {
    return;
  }

  pthread_mutex_lock((pthread_mutex_t*)(ctx->csm_mtx));

  memcpy(csm->hist_compr[csm->hist_compr_idx], entry,
             sizeof(struct limeade_csm_compression_entry));

  if(csm->hist_compr_idx == csm->hist_compr_sz - 1)
  {
    csm->hist_compr_idx = 0;
  } else
  {
    csm->hist_compr_idx++;
  }
  pthread_mutex_unlock((pthread_mutex_t*)(ctx->csm_mtx));
}

void limeade_csm_add_latency_entry(struct limeade_context *ctx,
                                   struct limeade_csm_latency_entry *entry)
{
  struct limeade_csm_data *csm = ctx->csm;

  if(csm == NULL)
  {
    return;
  }
  
  pthread_mutex_lock((pthread_mutex_t*)(ctx->csm_mtx));

  memcpy(csm->hist_latent[csm->hist_latent_idx], entry,
             sizeof(struct limeade_csm_latency_entry));

  if(csm->hist_latent_idx == csm->hist_latent_sz - 1)
  {
    csm->hist_latent_idx = 0;
  } else
  {
    csm->hist_latent_idx++;
  }

  pthread_mutex_unlock((pthread_mutex_t*)(ctx->csm_mtx));
}

