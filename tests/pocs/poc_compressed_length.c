#include<stdlib.h>

#include<zlib.h>

#include<liblimeade/liblimeade.h>
#include"poc_parser_packet.h"

int main(void)
{
  const unsigned char plain[] = {'x', LIMEADE_ROW_DELIM};
  unsigned char compressed[128];
  uLongf compressed_size = sizeof(compressed);
  if(compress2(compressed, &compressed_size, plain, sizeof(plain), Z_BEST_SPEED)
     != Z_OK || compressed_size < 2)
    return 1;

  struct limeade_context receiver = {0};
  struct limeade_recvd packet = {0};
  struct limeade_close out = {0};
  if(poc_receive_packet(LIMEADE_PACKET_CLOSE, 1, compressed,
                        compressed_size - 1, &receiver, &packet)
     != LIMEADE_SUCCESS)
    return 1;

  (void)limeade_parse_close(&out, &packet);
  limeade_release_recvd(&packet);
  limeade_destruct(&receiver);
  return 0;
}