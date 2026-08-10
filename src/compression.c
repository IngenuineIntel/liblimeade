// compression.c
// compression management for liblimeade
// [AGPL]

#include <stdlib.h>
#include <string.h>

#include <zconf.h>
#include <zlib.h>

#include <liblimeade/liblimeade.h>

void limeade_deflate_packet(struct limeade_packet_data *pkt, int compr_lvl)
{
  /* deflates into a separate buffer, then copies the deflated data over the
   * uncompressed data
   */

  if(compr_lvl <= 0)
  {
    limeade_inserr(LIMEADE_SUCCESS);
    return;
  }

  // step 1: compress into new buffer
  void *compressed = (Bytef*)malloc(pkt->pkt_sz);
  unsigned long compressed_len;
  if(compress2(compressed, &compressed_len, pkt->data, pkt->pkt_sz - sizeof(struct limeade_packet_flags), compr_lvl) != Z_OK)
  {
    free(compressed);
    limeade_inserr(LIMEADE_ERROR_COMPRESSION);
    return;
  }

  // step 2: copy new over old
  if(memcpy(pkt->data, compressed, compressed_len) != compressed_len)
  {
    free(compressed);
    limeade_inserr(LIMEADE_ERROR_COMPRESSION);
    return;
  }

  // step 3: realloc & free accordingly
  void *new_pkt = realloc(pkt->pkt, compressed_len + sizeof(struct limeade_packet_flags));

  if(new_pkt == NULL)
  {
    free(compressed);
    limeade_inserr(LIMEADE_ERROR_MEMORY);
    return;
  }
  pkt->pkt = new_pkt;
  pkt->data = new_pkt + sizeof(struct limeade_packet_flags);
  pkt->pkt_sz = compressed_len + sizeof(struct limeade_packet_flags);
  free(compressed);
  limeade_inserr(LIMEADE_SUCCESS);
}

// TODO deflation
