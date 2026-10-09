// state_check.c
//
// AGPL here

#include<signal.h>

#include<liblimeade/liblimeade-internal.h>

int limeade_statecheck(struct limeade_context *ctx)
{
  if(ctx->csm.enabled)
  {
    if(pthread_kill(ctx->csm.tid, 0) != 0)
      return LIMEADE_ERROR_TH_CSM_DIED;
  }

  if(ctx->mode == LIMEADE_MODE_CLIENT_SSH)
  {
    if(kill(ctx->ssh_pid, 0) != 0)
      return LIMEADE_ERROR_TH_RECV_DIED;
  }

  return LIMEADE_SUCCESS;
}

