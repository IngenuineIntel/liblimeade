#include<stdlib.h>
#include<string.h>

#include"poc_sender_context.h"

int main(void)
{
  struct limeade_context ctx = {0};
  char *explanation = malloc(LIMEADE_MAX_PKT_SZ + 1);
  if(!explanation)
    return 1;

  memset(explanation, 'A', LIMEADE_MAX_PKT_SZ);
  explanation[LIMEADE_MAX_PKT_SZ] = '\0';

  if(poc_init_sender(&ctx) != LIMEADE_SUCCESS)
  {
    free(explanation);
    return 1;
  }

  struct limeade_close close_data = {.explanation = explanation};
  (void)limeade_send(&ctx, LIMEADE_PACKET_CLOSE, close_data);

  limeade_destruct(&ctx);
  free(explanation);
  return 0;
}