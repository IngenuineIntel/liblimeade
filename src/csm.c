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

#include<liblimeade/liblimeade-internal.h>

void *limeade_csm(void *arg)
{
  // TODO
}

void limeade_csm_add_compr_entry(struct limeade_context *ctx,
                                 struct limeade_csm_compression_entry *entry)
{
  struct limeade_csm_data *csm = ctx->csm;

  if(!csm)
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

  if(!csm)
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

