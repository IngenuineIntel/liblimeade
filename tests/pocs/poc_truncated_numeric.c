#include<stdlib.h>

#include<liblimeade/liblimeade.h>
#include"poc_parser_packet.h"

int main(void)
{
  const unsigned char body[] = {
    0, LIMEADE_FIELD_DELIM,
    0, LIMEADE_FIELD_DELIM,
    0x20
  };
  struct limeade_context receiver = {0};
  struct limeade_recvd packet = {0};
  struct limeade_perf out = {0};

  if(poc_receive_packet(LIMEADE_PACKET_PERF, 0, body, sizeof(body),
                        &receiver, &packet) != LIMEADE_SUCCESS)
    return 1;

  (void)limeade_parse_perf(&out, &packet);
  limeade_release_recvd(&packet);
  limeade_destruct(&receiver);
  return 0;
}