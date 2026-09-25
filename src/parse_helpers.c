// parse_helpers.c
//
// Copyright (C) 2026 Roan Rothrock
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.


#include<emmintrin.h>
#include<stddef.h>
#include<string.h>

#include<zlib.h>

#include <liblimeade/liblimeade-internal.h>

int limeade_get_nr_rows(struct limeade_recvd *pkt)
{
  int ret = 0, rem  = pkt->pkt_sz - sizeof(struct limeade_packet_flags);
  void *next, *prev = pkt->data;
  for(;;)
  {
    next = memmem(prev, rem, (char*)LIMEADE_ROW_DELIM, 1);
    ret++;

    if(!next)
      return ret;

    rem -= (next - prev + 1);
    prev = next + 1;
  }
}

int limeade_decompress_packet(struct limeade_recvd pkt)
{
  uLongf new_l = 1 << 16;
  void *new = malloc((int)new_l);

  if(!new)
    return -1;

  struct limeade_packet_flags *f = pkt.flags;

  if(!f->compr_lvl)
    return 0;

  if(uncompress(new, &new_l, pkt.data, f->packet_size) != Z_OK)
  {
    free(new);
    return -1;
  }

  pkt.pkt_sz = sizeof(LIMEADE_MAGIC) + sizeof(*f) + new_l;
  pkt.pkt = realloc(pkt.pkt, pkt.pkt_sz);
  if(!pkt.pkt)
  {
    free(new);
    return -1;
  }

  pkt.flags = pkt.pkt   + sizeof(LIMEADE_MAGIC);
  pkt.data  = pkt.flags + sizeof(*f);
  memcpy(pkt.data, new, new_l);

  free(new);

  return 0;
}

// note: implementing this without SIMD would feel pretty gross, it's
// unconventional but improves performance relatively quickly
// also note that this uses SSE2 (16 byte) instead of AVX2 (32 byte) because I
// forsee the strings being passed into this function not being very long
uint32_t limeade_pkt_strlen(const char *s)
{
  __m128i fd, rd, chunk, m1, m2, mask;
  int match;
  uint32_t len = 0;

  fd = _mm_set1_epi8(LIMEADE_FIELD_DELIM);
  rd = _mm_set1_epi8(LIMEADE_ROW_DELIM);
  len = 0;

  // aligning to memory page boundary
  while(((uintptr_t)s & 15) && *s != LIMEADE_FIELD_DELIM
                            && *s != LIMEADE_ROW_DELIM)
  {
    s++;
    len++;
  }

  for(;;)
  {
    chunk = _mm_load_si128((const __m128i*)s);
    m1    = _mm_cmpeq_epi8(chunk, fd);
    m2    = _mm_cmpeq_epi8(chunk, rd);
    mask  = _mm_or_si128(m1, m2);
    match = _mm_movemask_epi8(mask);

    if(match)
    {
      len += __builtin_ctz(match);
      break;
    }
    s   += 16;
    len += 16;
  }
  return len;
}

