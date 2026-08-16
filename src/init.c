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

          execlp("ssh", "ssh", "-s", LIMEADE_SUBSYSTEM_NAME, dest, (char*)NULL);
          exit(-1);
          
        }
        default:
          CHECK(close(fr_ssh[1]) == -1, LIMEADE_ERROR_OTHER);
          CHECK(close(to_ssh[0]) == -1, LIMEADE_ERROR_OTHER);
          ret->rfd = fr_ssh[0];
          ret->sfd = to_ssh[1];

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

  }




  limeade_inserr(LIMEADE_SUCCESS);
  return ret;

  err:
  free(ret);
  return NULL;
}

