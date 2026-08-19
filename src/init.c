// init.c
// AGPL

#define CHECK(expr, err) \
if(expr)                 \
{                        \
  limeade_inserr(err);   \
  goto err;              \
}
#define CHILDCHECK(expr)\
if(expr)exit(-1);
struct limeade_context *limeade_init(uint8_t flags, ...)
{
  va_list arg;
  va_start(arg, flags);

  struct limeade_context *ret = (struct limeade_context*)malloc(sizeof(struct limeade_context));

  ret->mode       = flags & 0b00001111;
  ret->compr_mode = flags & 0b11110000;

  uint8_t allow_compr = 1;

  switch(ret->mode)
  {
    case LIMEADE_MODE_HOST_SSH:
    {
      ret->rfd = STDIN_FILENO;
      ret->sfd = STDOUT_FILENO;
      allow_compr = 0;
    }

    case LIMEADE_MODE_CLIENT_SSH:
    {
      const char *dest = va_arg(arg, const char*);
      ctx->dest = malloc(strlen(dest) + 1);
      strcpy(ctx->dest, dest);

      int to_ssh[2];
      int fr_ssh[2];

      CHECK(pipe(to_ssh) == -1, LIMEADE_ERROR_OTHER);
      CHECK(pipe(fr_ssh) == -1, LIMEADE_ERROR_OTHER);

      ctx->ssh_pid = fork();

      switch(ctx->ssh_pid)
      {
        case -1:
          CHECK(1 == 1, LIMEADE_ERROR_OTHER);
        case 0:
        {
          CHILDCHECK(dup2(fr_ssh[1], STDOUT_FILENO) != 0);
          CHILDCHECK(dup2(to_ssh[0], STDIN_FILENO)  != 0);

          close(fr_ssh[0]);
          close(fr_ssh[1]);
          close(to_ssh[0]);
          close(to_ssh[1]);

          execlp("ssh", "ssh", "-s",
                 LIMEADE_SUBSYSTEM_NAME, dest, (char*)NULL);
          exit(-1);
          
        }
        default:
          CHECK(close(fr_ssh[1]) == -1, LIMEADE_ERROR_OTHER);
          CHECK(close(to_ssh[0]) == -1, LIMEADE_ERROR_OTHER);
          ret->rfd = fr_ssh[0];
          ret->sfd = to_ssh[1];

      }
    }
    case LIMEADE_MODE_CLIENT_ETH:
    {
      const char *dest = va_arg(arg, const char*);
      ctx->dest        = malloc(strlen(dest) + 1);
      ctx->port        = va_arg(arg, int);
      strcpy(ctx->dest, dest);

      ret->sfd = socket(AF_INET, SOCK_DGRAM, 0);
      CHECK(ret->sfd < 0, LIMEADE_ERROR_NETWORK);
      ret->rfd = ret->sfd;

      memset(&ret->saddr, 0, sizeof(struct sockaddr_in));

      ret->saddr.sin_family      = AF_INET;
      ret->saddr.sin_port        = ctx->port;
      ret->saddr.sin_addr.s_addr = inet_addr(dest);

      ret->saddr_len = sizeof(ret->saddr);
    }
    case LIMEADE_MODE_HOST_ETH:
    {
      ctx->saddr = malloc(sizeof(struct sockaddr_in));
      CHECK(ctx->saddr == NULL, LIMEADE_ERROR_MEMORY);
      ctx->cliaddr = malloc(sizeof(struct sockaddr_in));
      CHECK(ctx->saddr == NULL, LIMEADE_ERROR_MEMORY);

      ctx->port = va_arg(arg, int);

      ret->rfd = socket(AF_INET, SOCK_DGRAM, 0);
      CHECK(ret->rfd < 0, LIMEADE_ERROR_NETWORK);
      ret->sfd = ret->rfd;

      ctx->saddr->sin_adr.s_addr = htonl(INADDR_ANY);
      ctx->saddr->sin_port = htons(ctx->port);
      ctx->saddr->sin_family = AF_INET;


      allow_compr = 0;
    }
  }

  if(allow_compr != 0)
  {
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
        ret->csm->hist_compr_sz = LIMEADE_CSM_BENCH_BUFFER_SIZE;
        ret->csm->hist_bandw_sz = LIMEADE_CSM_BENCH_BUFFER_SIZE;
        ret->csm->freq_s        = LIMEADE_CSM_FREQ_S;
    }
  }

  ctx->mtx_sfd        = malloc(sizeof(pthread_mutex_t));
  CHECK(ctx->mtx_sfd == NULL, LIMEADE_ERROR_MEMORY);
  ctx->mtx_rfd        = malloc(sizeof(pthread_mutex_t));
  CHECK(ctx->mtx_rfd == NULL, LIMEADE_ERROR_MEMORY);
  ctx->mtx_mode_union = malloc(sizeof(pthread_mutex_t));
  CHECK(ctx->mtx_mode_union == NULL, LIMEADE_ERROR_MEMORY);
  ctx->mtx_comp       = malloc(sizeof(pthread_mutex_t));
  CHECK(ctx->mtx_comp == NULL, LIMEADE_ERROR_MEMORY);
  ctx->mtx_th_csm     = malloc(sizeof(pthread_mutex_t));
  CHECK(ctx->mtx_th_csm == NULL, LIMEADE_ERROR_MEMORY);
  ctx->mtx_pub        = malloc(sizeof(pthreaD_mutex_t));
  CHECK(ctx->mtx_pub == NULL, LIMEADE_ERROR_MEMORY);

  pthread_mutex_init(ctx->mtx_sfd, NULL);
  pthread_mutex_init(ctx->mtx_rfd, NULL);
  pthread_mutex_init(ctx->mtx_mode_union, NULL);
  pthread_mutex_init(ctx->mtx_comp, NULL);
  pthread_mutex_init(ctx->mtx_th_csm, NULL);
  pthread_mutex_init(ctx->mtx_pub, NULL);

  limeade_inserr(LIMEADE_SUCCESS);
  return ret;

  err:
  free(ret);
  // TODO real error handling
  return NULL;
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
      // TODO
    case LIMEADE_MODE_HOST_SSH:
      // TODO
    case LIMEADE_MODE_CLIENT_SSH:
      // TODO
  }

  if(ret->csm != NULL)
  {
    ret->csm->hist_compr_benches = malloc(sizeof(uint32_t) * LIMEADE_CSM_BENCH_BUFFER_SIZE);
    ret->csm->hist_bandw_benches = malloc(sizeof(uint32_t) * LIMEADE_CSM_BENCH_BUFFER_SIZE);

    CHECK(ret->csm->hist_compr_benches == NULL, LIMEADE_ERROR_MEMORY);
    CHECK(ret->csm->hist_bandw_benches == NULL, LIMEADE_ERROR_MEMORY);
    
    ret->csm->hist_compr_idx = 0;
    ret->csm->hist_bandw_idx = 0;

    ret->csm->id = malloc(sizeof(pthread_t));
    CHECK(ret->csm->id == NULL, LIMEADE_ERROR_MEMORY);

    CHECK(
      pthread_create(ret->csm->id, NULL, limeade_csm, (void*)ret) != 0,
      LIMEADE_ERROR_CSM);
  }

  // TODO recv thread

  // TODO Limeade handshake

}
