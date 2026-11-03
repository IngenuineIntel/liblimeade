#ifndef LIMEADE_POC_SENDER_CONTEXT_H
#define LIMEADE_POC_SENDER_CONTEXT_H

#include<string.h>

#include<liblimeade/liblimeade.h>

static inline int poc_init_sender(struct limeade_context *ctx)
{
  memset(ctx, 0, sizeof(*ctx));
  int result = limeade_init(ctx,
      LIMEADE_MODE_CLIENT_ETH | LIMEADE_MODE_NO_COMPRESSION,
      "127.0.0.1", 9);
  if(result != LIMEADE_SUCCESS)
    return result;
  return limeade_connect(ctx);
}

#endif