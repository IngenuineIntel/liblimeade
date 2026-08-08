// csm.c
//  X compatibility support module
//  X certified scrum master
// -> compression supervisor module

#include <time.h>

#include <liblimeade/liblimeade.h>

void limeade_monotonic(struct timespec *ts)
{
  if(clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
  {
    limeade_inserr(LIMEADE_ERROR_MONOTONIC);
  } else
  {
    limeade_inserr(LIMEADE_SUCCESS);
  }
}

unsigned int limeade_monotonic_diff_ms(struct timespec *a, struct timespec *b)
{
  int ret;
  int ms;

  ret = b.tv_sec - a.tv_sec;

  // in theory, b should be the latter timestamp, but just in case
  if(ret < 0)
  {
    ret = 0 - ret;
  }

  ret *= 1000;

  ms = (b.tv_nsec - a.tv_nssec) / 1000;

  if(ms < 0)
  {
    ms = 0 - ms;
  }
  
  ret += ms;
  return ret;

}

void limeade_csm_add_compr_entry(struct limeade_context *ctx,
                                 struct limeade_csm_compression_entry *entry)
{
  int r;
  struct limeade_csm_data *csm = ctx->csm;

  pthread_mutex_lock((pthread_mutex_t*)ctx->csm_mtx);
  
  r = memcpy(csm->hist_compr[csm->hist_compr_idx], entry,
             sizeof(struct limeade_csm_compression_entry));

  if(r != sizeof(struct limeade_csm_compression_entry))
  {
    // assuming the entry is ruined, so decrementing the index
    if(csm->hist_compr_idx == 0)
    {
      csm->hist_compr_idx = csm->hist_compr_sz - 1;
    } else
    {
      csm->hist_compr_idx--;
    }
  
    pthread_mutex_unlock((pthread_mutex_t*)ctx->csm_mtx);
    limeade_inserr(LIMEADE_ERROR_OTHER);
    return;
  }

  if(csm->hist_compr_idx == csm->hist_compr_sz - 1)
  {
    csm->hist_compr_idx = 0;
  } else
  {
    csm->hist_compr_idx++;
  }
  pthread_mutex_unlock((pthread_mutex_t*)ctx->csm_mtx);
  limeade_inserr(LIMEADE_SUCCESS);
}

void limeade_csm_add_latency_entry(struct limeade_context *ctx,
                                   struct limeade_csm_latency_entry *entry)
{
  int r;
  struct limeade_csm_data *csm = ctx->csm;
  
  pthread_mutex_lock((pthread_mutex_t*)ctx->csm_mtx);

  r = memcpy(csm->hist_latency[csm->hist_latency_idx], entry,
             sizeof(struct limeade_csm_latency_entry));

  if(r != sizeof(struct limeade_csm_latency_entry))
  {
    // assuming the entry is ruined, so decrementing the index
    if(csm->hist_latency_idx == 0)
    {
      csm->hist_latency_idx = csm->hist_latency_sz - 1;
    } else
    {
      csm->hist_latency_idx--;
    }
    pthread_mutex_unlock((pthread_mutex_t*)ctx->csm_mtx);
    limeade_inerr(LIMEADE_ERROR_OTHER);
    return;
  }

  if(csm->hist_latency_idx == csm->hist_latency_sz - 1)
  {
    csm->hist_latency_idx = 0;
  } else
  {
    csm->hist_latency_idx++;
  }

  pthread_mutex_unlock((pthread_mutex_t*)ctx->csm_mtx);
  limeade_inserr(LIMEADE_SUCCESS);
}

void limeade_csm(void *arg)
{
  // TODO
}

