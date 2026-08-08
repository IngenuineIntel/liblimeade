// compression.c
// compression management for liblimeade
// [AGPL]

#include <zlib.h>

#include <liblimeade/liblimeade.h>

void limeade_deflate_packet(struct limeade_packet_data *in, int compr_lvl)
{
  /* deflates into a separate buffer, then copies the deflated data over the
   * uncompressed data
   */

  if(compr_lvl <= 0)
  {
    limeade_inserr(LIMEADE_SUCCESS);
    return 0;
  }

  // step 1: compress into new buffer
  char *compressed = (char*)malloc(pkt->pkt_sz);
  unsigned int compressed_len;
  if(compress2(*compressed, &compressed_len, pkt->data, pkt->pkt_sz - sizeof(limeade_packet_flags), compr_lvl) != Z_OK)
  {
    free(compressed);
    limeade_inserror(LIMEADE_ERROR_COMPRESSION);
    return;
  }

  // step 2: copy new over old
  if(memcpy(pkt->data, compressed, compresed_len) != compressed_len)
  {
    free(compressed);
    limeade_inserror(LIMEADE_ERROR_COMPRESSION);
    return;
  }

  // step 3: realloc & free accordingly
  void *new_pkt = realloc(in->pkt, compressed_len + sizeof(limeade_packet_flags));

  if(new_pkt == NULL)
  {
    free(compressed);
    limeade_inserror(LIMEADE_ERROR_MEMORY);
    return;
  }
  in->pkt = new_pkt;
  in->data = new_pkt + sizeof(limeade_packet_flags);
  in->pkt_sz = compressed_len + sizeof(limeade_packet_flags);
  free(compressed);
  liemade_inserr(LIMEADE_SUCCESS);
}

// TODO deflation
