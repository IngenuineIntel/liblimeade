// init.c
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

#include<stdarg.h>
#include<stdlib.h>

#include<liblimeade/liblimeade-internal.h>

int limeade_init(struct limeade_context *ctx, uint8_t flags, ...)
{
  int ret;
  va_list arg;
  va_start(arg, flags);

  ctx->mode       = flags & 0b00001111;
  ctx->compr_mode = flags & 0b11110000;

  uint8_t allow_compr = 1;

  switch(ctx->mode)
  {
    case LIMEADE_MODE_HOST_SSH:
    {
      ctx->rfd = STDIN_FILENO;
      ctx->sfd = STDOUT_FILENO;
      allow_compr = 0;
    }

    case LIMEADE_MODE_CLIENT_SSH:
    {
      const char *dest = va_arg(arg, const char*);
      ctx->dest = malloc(strlen(dest) + 1);
      strcpy(ctx->dest, dest);

      int to_ssh[2];
      int fr_ssh[2];

      if(pipe(to_ssh) == -1)
      {
        ret = LIMEADE_ERROR_OTHER;
        goto err;
      }

      if(pipe(fr_ssh) == -1)
      {
        ret = LIMEADE_ERROR_OTHER;
        goto err;
      }

      ctx->ssh_pid = fork();

      switch(ctx->ssh_pid)
      {
        case -1:
          ret = LIMEADE_ERROR_SSH;
          goto err;
        case 0:
        {
          dup2(fr_ssh[1]);
          dup2(to_ssh[0]);

          close(fr_ssh[0]);
          close(fr_ssh[1]);
          close(to_ssh[0]);
          close(to_ssh[1]);

          execlp("ssh", "ssh", "-s",
                 LIMEADE_SUBSYSTEM_NAME, dest, (char*)NULL);
          exit(-1);
        }
        default:
          close(fr_ssh[1]);
          close(to_ssh[0]);
          ctx->rfd = fr_ssh[0];
          ctx->sfd = to_ssh[1];
      }
    }
    case LIMEADE_MODE_CLIENT_ETH:
    {
      const char *dest = va_arg(arg, const char*);
      ctx->dest        = malloc(strlen(dest) + 1);
      if(ctx->dest == NULL)
      {
        ret = LIMEADE_ERROR_MEMORY;
        goto err;
      }
      ctx->port        = va_arg(arg, int);
      strcpy(ctx->dest, dest);

      ctx->sfd = socket(AF_INET, SOCK_DGRAM, 0);
      if(ctx->sfd < 0)
      {
        ret = LIMEADE_ERROR_MEMORY;
        goto ret;
      }
      ctx->rfd = ctx->sfd;

      ctx->saddr = malloc(sizeof(struct sockaddr_in));
      if(ctx->saddr == NULL)
      {
        ret = LIMEADE_ERROR_MEMORY;
        goto ret;
      }

      memset(&ctx->saddr, 0, sizeof(struct sockaddr_in));

      ctx->saddr.sin_family      = AF_INET;
      ctx->saddr.sin_port        = ctx->port;
      ctx->saddr.sin_addr.s_addr = inet_addr(dest);

      ctx->saddr_len = sizeof(ctx->saddr);
    }
    case LIMEADE_MODE_CLIENT_LIBSSH:
    {
#ifdef LIMEADE_HAS_LIBSSH2
      // TODO
#else
      return LIMEADE_ERROR_NOT_SUPPORTED;
#endif /* LIMEADE_HAS_LIBSSH2 */
    }
    case LIMEADE_MODE_HOST_ETH:
    {
      // TODO handlers for client list buffer
      ctx->saddr   = malloc(sizeof(struct sockaddr_in));
      if(ctx->saddr == NULL)
      {
        ret = LIMEADE_ERROR_MEMORY;
        goto err;
      }

      ctx->cliaddr = malloc(sizeof(struct sockaddr_in));
      if(ctx->cliaddr == NULL)
      {
        ret = LIMEADE_ERROR_MEMORY;
        goto err;
      }

      ctx->port = va_arg(arg, int);

      ctx->rfd = socket(AF_INET, SOCK_DGRAM, 0);
      ctx->sfd = ctx->rfd;

      if(ctx->rfd < 0)
      {
        ret = LIMEADE_ERROR_NETWORK;
        goto err;
      }

      ctx->saddr->sin_adr.s_addr = htonl(INADDR_ANY);
      ctx->saddr->sin_port = htons(ctx->port);
      ctx->saddr->sin_family = AF_INET;

      allow_compr = 0;
    }
  }

  if(allow_compr != 0)
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
        ctx->csm = (struct limeade_csm_data*)malloc(sizeof(limeade_csm_data));
        ctx->csm->hist_compr_sz = LIMEADE_CSM_BENCH_BUFFER_SIZE;
        ctx->csm->hist_bandw_sz = LIMEADE_CSM_BENCH_BUFFER_SIZE;
        ctx->csm->freq_s        = LIMEADE_CSM_FREQ_S;
    }
  }

  ctx->mtx_sfd        = malloc(sizeof(pthread_mutex_t));
  ctx->mtx_rfd        = malloc(sizeof(pthread_mutex_t));
  ctx->mtx_mode_union = malloc(sizeof(pthread_mutex_t));
  ctx->mtx_comp       = malloc(sizeof(pthread_mutex_t));
  ctx->mtx_th_csm     = malloc(sizeof(pthread_mutex_t));
  ctx->mtx_pub        = malloc(sizeof(pthreaD_mutex_t));

  if(ctx->mtx_sfd == NULL  || ctx->mtx_rfd == NULL    || ctx->mtx_mode_union == NULL\
  || ctx->mtx_comp == NULL || ctx->mtx_th_csm == NULL || ctx->mtx_pub == NULL)
  {
    ret = LIMEADE_ERROR_MEMORY;
    goto ret;
  }

  pthread_mutex_init(ctx->mtx_sfd, NULL);
  pthread_mutex_init(ctx->mtx_rfd, NULL);
  pthread_mutex_init(ctx->mtx_mode_union, NULL);
  pthread_mutex_init(ctx->mtx_comp, NULL);
  pthread_mutex_init(ctx->mtx_th_csm, NULL);
  pthread_mutex_init(ctx->mtx_pub, NULL);

  return LIMEADE_SUCCESS;

  err:
  // note: these `free`s are safe becasue `free` does a NULL check before
  // attempting to unmap

  free(ctx->mtx_sfd);
  free(ctx->mtx_rfd);
  free(ctx->mtx_mode_union);
  free(ctx->mtx_comp);
  free(ctx->mtx_th_csm);
  free(ctx->mtx_pub);

  switch(ctx->mode)
  {
    case LIMEADE_MODE_CLIENT_SSH:
      free(ctx->dest);
      kill(ctx->ssh_pid, SIGKILL); // don't care about failure
    case LIMEADE_MODE_CLIENT_ETH:
      free(ctx->dest);
      free(ctx->saddr);
    case LIMEADE_MODE_HOST_ETH:
      free(ctx->saddr);
      free(ctx->cliaddr); // to be changed
  }

  free(ctx->csm);

  return ret;
}

int limeade_connect(struct limeade_context *ctx)
{
  switch(ctx->mode)
  {
    case LIMEADE_MODE_HOST_ETH:
      // TODO
    case LIMEADE_MODE_CLIENT_ETH:
      // TODO
    case LIMEADE_MODE_CLIENT_LIBSSH:
#ifdef LIMEADE_HAS_LIBSSH2
      // TODO
#else
      return LIMEADE_ERROR_NOT_SUPPORTED;
#endif /* LIMEADE_HAS_LIBSSH2 */
    case LIMEADE_MODE_HOST_SSH:
      // TODO
    case LIMEADE_MODE_CLIENT_SSH:
      // TODO
  }

  if(ctx->csm != NULL)
  {
    // TODO eliminate CHECK
    ctx->csm->hist_compr_benches = malloc(sizeof(uint32_t) * LIMEADE_CSM_BENCH_BUFFER_SIZE);
    ctx->csm->hist_bandw_benches = malloc(sizeof(uint32_t) * LIMEADE_CSM_BENCH_BUFFER_SIZE);

    CHECK(ctx->csm->hist_compr_benches == NULL, LIMEADE_ERROR_MEMORY);
    CHECK(ctx->csm->hist_bandw_benches == NULL, LIMEADE_ERROR_MEMORY);

    ctx->csm->hist_compr_idx = 0;
    ctx->csm->hist_bandw_idx = 0;

    ctx->csm->id = malloc(sizeof(pthread_t));
    CHECK(ctx->csm->id == NULL, LIMEADE_ERROR_MEMORY);

    CHECK(
      pthread_create(ctx->csm->id, NULL, limeade_csm, (void*)ctx) != 0,
      LIMEADE_ERROR_CSM);
  }

  // TODO recv thread

  // TODO Limeade handshake

}
