// csm.c
//  X compatibility support module
//  X certified scrum master
// -> compression supervisor module

#include <pthread.h>
#include <string.h>
#include <time.h>

#include <liblimeade/liblimeade.h>

void limeade_monotonic(struct timespec *ts)
{
  if(clock_gettime(CLOCK_MONOTONIC, ts) != 0)
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

  ret = b->tv_sec - a->tv_sec;

  // in theory, b should be the latter timestamp, but just in case
  if(ret < 0)
  {
    ret = 0 - ret;
  }

  ret *= 1000;

  ms = (b->tv_nsec - a->tv_nsec) / 1000;

  if(ms < 0)
  {
    ms = 0 - ms;
  }

  ret += ms;
  return ret;

}

void limeade_csm_add_compr_entry(void *ctx,
                                 struct limeade_csm_compression_entry *entry)
{
  struct limeade_csm_data *csm = ((struct limeade_context*)ctx)->csm;

  pthread_mutex_lock((pthread_mutex_t*)((struct limeade_context*)ctx)->csm_mtx);

  memcpy(csm->hist_compr[csm->hist_compr_idx], entry,
             sizeof(struct limeade_csm_compression_entry));

  if(csm->hist_compr_idx == csm->hist_compr_sz - 1)
  {
    csm->hist_compr_idx = 0;
  } else
  {
    csm->hist_compr_idx++;
  }
  pthread_mutex_unlock((pthread_mutex_t*)((struct limeade_context*)ctx)->csm_mtx);
  limeade_inserr(LIMEADE_SUCCESS);
}

void limeade_csm_add_latency_entry(void *ctx,
                                   struct limeade_csm_latency_entry *entry)
{
  struct limeade_csm_data *csm = ((struct limeade_context*)ctx)->csm;

  pthread_mutex_lock((pthread_mutex_t*)((struct limeade_context*)ctx)->csm_mtx);

  memcpy(csm->hist_latent[csm->hist_latent_idx], entry,
             sizeof(struct limeade_csm_latency_entry));

  if(csm->hist_latent_idx == csm->hist_latent_sz - 1)
  {
    csm->hist_latent_idx = 0;
  } else
  {
    csm->hist_latent_idx++;
  }

  pthread_mutex_unlock((pthread_mutex_t*)((struct limeade_context*)ctx)->csm_mtx);
  limeade_inserr(LIMEADE_SUCCESS);
}

void limeade_csm(void *arg)
{
  // TODO
}
