// init.c
//
// [AGPL here]

#include <stdarg.h>
#include <stdlib.h>
#include <pthread.h>

#include <liblimeade/liblimeade.h>

void *limeade_csm(void *arg)
{
  struct limeade_context ctx = *(struct limeade_context*)arg;

  // TODO
  
  return NULL;
}

#define CHECK(expr, err) \
if(expr)                 \
{                        \
  limeade_inserror(err); \
  goto err;              \
}
#define CHILDCHECK(expr, err)\
if(expr){exit(EXIT_FAILURE);}

struct limeade_context *limeade_init(uint8_t flags, ...)
{

  va_list arg;
  va_start(arg, flags);

  struct limeade_context *ret = (struct limeade_context*)malloc(sizeof(struct limeade_context));

  ret->mode = flags & 00001111;
  ret->compr_mode = flags & 11110000;

  uint8_t allow_compr = 1;

  switch(ret->mode)
  {
    case LIMEADE_MODE_HOST_SSH:
    {
      ret->rfd = STDOUT_FILENO;
      ret->sfd = STDOUT_FILENO;
      allow_compr = 0;
    }
    case LIMEADE_MODE_CLIENT_SSH:
    {
      const char *dest = va_arg(arg, const char*);

      int ssh_in[2];
      int ssh_out[2];

      CHECK(pipe(to_ssh) == -1, LIMEADE_ERROR_OTHER);
      CHECK(pipe(fr_ssh) == -1, LIMEADE_ERROR_OTHER);

      switch(fork())
      {
        case -1:
          CHECK(1 == 1, LIMEADE_ERROR_OTHER);
        case 0:
          CHILDCHECK(dup2(ssh_out[1], STDOUT_FILENO) == 1, LIMEADE_ERROR_OTHER);
          CHILDCHECK(dup2(ssh_in[0], STDIN_FILENO) == 1, LIMEADE_ERROR_OTHER);

          CHILDCHECK(close(ssh_out[0]) == -1, LIMEADE_ERROR_OTHER);
          CHILDCHECK(close(ssh_out[1]) == -1, LIMEADE_ERROR_OTHER);
          CHILDCHECK(close(ssh_in[0]) == -1, LIMEADE_ERROR_OTHER);
          CHILDCHECK(close(ssh_in[1]) == -1, LIMEADE_ERROR_OTHER);

          execlp("ssh", "ssh", "-s", LIMEADE_SUBSYSTEM_NAME, dest, (char*)NULL);
          exit(-1);
        default:
          CHECK(close(ssh_out[1]) == -1, LIMEADE_ERROR_OTHER);
          CHECK(close(ssh_in[0]) == -1, LIMEADE_ERROR_OTHER);
          ret->rfd = ssh_out[0];
          ret->sfd = ssh_in[1];
      }
    }
    case LIMEADE_MODE_CLIENT_LIBSSH:
    {
      // TODO
    }
    case LIMEADE_MODE_HOST_ETH:
    {
      // TODO
      
      struct sockaddr_in s;

      ret->rfd = socket(AF_INET, SOCK_DGRAM, 0);
      CHECK(ret->sfd < 0, LIMEADE_ERROR_NETWORK);
      ret->sfd = ret->rfd;

      

      s.sin_family = AF_INET;
      s.sin_addr.s_addr = INADDR_ANY;
      s.sin_port = htons(PORT);

      CHECK(
        bind(ret->rfd, (const struct sockaddr*)&servaddr, sizeof(servaddr)) < 0,
        LIMEADE_ERROR_NETWORK
      );

    }
    case LIMEADE_MODE_CLIENT_ETH:
    {
      const char *dest = va_arg(arg, const char*);
      int port         = va_arg(arg, int);

      ret->sfd = socket(AF_INET, SOCK_DGRAM, 0);
      CHECK(ret->sfd < 0, LIMEADE_ERROR_NETWORK);
      ret->rfd = ret->sfd;

      memset(&ret->saddr, 0, sizeof(ret->server_addr));

      ret->saddr.sin_family = AF_INET;
      ret->saddr.sin_port   = port;
      ret->saddr.sin_addr.s_addr = inet_addr(dest);

      socklen_t ret->saddr_len = sizeof(ret->saddr);
    }
  }

  switch(ret->compr_mode)
  {
    case LIMEADE_MODE_NO_COMPRESSION:
      ret->compr_lvl = 0;
    case LIMEADE_MODE_LOW_COMPRESSION:
      ret->compr_lvl = 1;
    case LIMEADE_MODE_MED_COMPRESSION:
      ret->compr_lvl = 4;
    case LIMEADE_MODE_HIGH_COMPRESSION:
      ret->compr_lvl = 7;
    default: 
      ret->csm = (struct limeade_csm_data*)malloc(sizeof(limeade_csm_data));

      CHECK(ret->csm == NULL, LIMEADE_ERROR_MEMORY);

      ret->csm->hist_compr_sz = LIMEADE_CSM_BENCH_BUFFER_SIZE;
      ret->csm->hist_bandw_sz = LIMEADE_CSM_BENCH_BUFFER_SIZE;
      ret->csm->hist_compr_benches = malloc(sizeof(uint32_t) * LIMEADE_CSM_BENCH_BUFFER_SIZE);
      ret->csm->hist_bandw_benches = malloc(sizeof(uint32_t) * LIMEADE_CSM_BENCH_BUFFER_SIZE);
  
      CHECK(ret->csm->hist_compr_benches == NULL, LIMEADE_ERROR_MEMORY);
      CEHCK(ret->csm->hist_bandw_benches == NULL, LIMEADE_ERROR_MEMORY);

      ret->csm->hist_compr_idx = 0;
      ret->csm->hist_bandw_idx = 0;

      ret->csm->freq_s = LIMEADE_CSM_FREQ_S;
      ret->csm->id = malloc(sizeof(pthread_t));

      CHECK(ret->csm->id == NULL, LIMEADE_ERROR_MEMORY);

      CHECK(
        pthread_create(ret->csm->id, NULL, limeade_csm, (void*)ret) != 0,
        LIMEADE_ERROR_CSM
      );
  }

  // TODO

  return ret;

  err:
  if (ret->csm != NULL)
  {
    free(ret->csm->hist_compr_benches);
    free(ret->csm->hist_bandw_benches);
  }
  free(ret->csm);
  free(ret);
  return (LIMEADE_CONTEXT*)NULL;
}

int limeade_connect(struct limeade_context *ctx)
{

  switch(ctx->mode)
  {
    case LIMEADE_MODE_HOST_SSH:
    {
      return 0; // nothing to do
    }
    case LIMEADE_MODE_CLIENT_SSH:
    {
      // the SSH connection has already been made, but the Limeade connection
      // has not


      // TODO
    }
    case LIMEADE_MODE_CLIENT_LIBSSH:
    {
      // TODO
    }
    case LIMEADE_MODE_HOST_ETH:
    {
      return 0;
    }
    case LIMEADE_MODE_CLIENT_ETH:
    {
      // TODO
    }
  }
}

