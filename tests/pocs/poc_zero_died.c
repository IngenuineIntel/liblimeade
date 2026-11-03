#include<stdlib.h>

#include"poc_sender_context.h"

int main(void)
{
  struct limeade_context ctx = {0};
  struct limeade_proc_update update = {0};

  if(poc_init_sender(&ctx) != LIMEADE_SUCCESS)
    return 1;

  update.total_died = 0;
  update.total_altered = 0;
  update.died = NULL;
  (void)limeade_send(&ctx, LIMEADE_PACKET_PROC_UPDATE, update);

  limeade_destruct(&ctx);
  return 0;
}