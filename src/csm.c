// csm.c
//
// Copyright (C) 2026 Roan Rothrock
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include<pthread.h>

#include<liblimeade/liblimeade-internal.h>

// TODO
void *limeade_th_csm(void *arg)
{
  struct timespec rqtp, rmtp;
  /*
  struct limeade_context *ctx;
  float b;

  ctx = arg;

  switch(ctx->compr_mode)
  {
    case LIMEADE_MODE_CSM_SAVE_CYCLES:
      b = 0.2;
    case LIMEADE_MODE_CSM_SAVE_ALL:
      b = 0.5;
    case LIMEADE_MODE_CSM_SAVE_THROUGHPUT:
      b = 0.8;
    default:
      return NULL;
  }
  */

  rqtp.tv_sec = 5;

  for(;;)
    nanosleep(&rqtp, &rmtp);

  return NULL;
}
// The following is the end-goal of this function. It is commented out currently
// because I do not intend on immediately implementing `limeade_csm`.
/*
void limeade_csm_add_compr_entry(struct limeade_context *ctx,
                                 struct limeade_csm_compression_entry *entry)
{
  struct limeade_csm_data *csm = ctx->csm;

  if(!csm)
    return;

  pthread_mutex_lock(ctx->mtx_th_csm);

  memcpy(&csm->hist_compr[csm->hist_compr_idx], entry,
         sizeof(struct limeade_csm_compression_entry));

  if(csm->hist_compr_idx == csm->hist_compr_sz - 1)
    csm->hist_compr_idx = 0;
  else
    csm->hist_compr_idx++;

  pthread_mutex_unlock(ctx->mtx_th_csm);
}
*/

// Same goes for this function.
/*
void limeade_csm_add_latency_entry(struct limeade_context *ctx,
                                   struct limeade_csm_latency_entry *entry)
{
  struct limeade_csm_data *csm = ctx->csm;

  if(!csm)
    return;
  
  pthread_mutex_lock(ctx->mtx_th_csm);

  memcpy(&csm->hist_latent[csm->hist_latent_idx], entry,
             sizeof(struct limeade_csm_latency_entry));

  if(csm->hist_latent_idx == csm->hist_latent_sz - 1)
    csm->hist_latent_idx = 0;
  else
    csm->hist_latent_idx++;

  pthread_mutex_unlock(ctx->mtx_th_csm);
}
*/

// Alternatively...
// In theory the data construction that happens in `limeade_send*` will get
// optimized out with the functions being nothingburgers like this.
void limeade_csm_add_compr_entry(struct limeade_context *ctx,
                                 struct limeade_csm_compression_entry *entry)
{
  return;
}

void limeade_csm_add_latency_entry(struct limeade_context *ctx,
                                   struct limeade_csm_latency_entry *entry)
{
  return;
}
