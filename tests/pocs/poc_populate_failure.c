#include<stdlib.h>

#include"poc_sender_context.h"

int main(void)
{
  const uint16_t event_count = 14000;
  struct limeade_context ctx = {0};
  struct limeade_indiv_event *items = calloc(event_count, sizeof(*items));
  if(!items)
    return 1;

  if(poc_init_sender(&ctx) != LIMEADE_SUCCESS)
  {
    free(items);
    return 1;
  }

  struct limeade_events events = {
    .nr_events = event_count,
    .events = items
  };
  (void)limeade_send(&ctx, LIMEADE_PACKET_EVENTS, events);

  limeade_destruct(&ctx);
  free(items);
  return 0;
}