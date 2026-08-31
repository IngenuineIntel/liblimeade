// liblimeade-internal.h
// internal functions for liblimeade
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

#ifndef _LIBLIMEADE_INTERNAL_H_
#define _LIBLIMEADE_INTERNAL_H_

#include<liblimeade/liblimeade.h>

/* limeade_monotonic
 *
 * wrapper for monotonic timestamps
 */
int limeade_monotonic(struct timespec *ts);

/* limeade_monotonic_diff_ms
 *
 * gets milliseconds between `a` & `b`
 */
int64_t limeade_monotonic_diff_ms(struct timespec *a, struct timespec *b);

/* limeade_csm_add_compr_entry
 *
 * add `entry` to CSM benchmark data, or exits if CSM isn't active
 */
void limeade_csm_add_compr_entry(struct limeade_context *ctx,
                                  struct limeade_csm_compression_entry *entry);

/* limeade_csm_add_latency_entry
 *
 * add `entry` to CSM benchmark data, or exists if CSM isn't active
 */
void limeade_csm_add_latency_entry(struct limeade_context *ctx,
                                   struct limeade_csm_latency_entry *entry);

/* limeade_get_rows_in_packet
 *
 * returns the number of rows in `in` (rows being separated by LIMEADE_ROW_DELIM)
 */
int limeade_get_rows_in_packet(struct limeade_recvd in);

// the following macros are for creating functions used for parsing data from
// packets
#define LIMEADE_TYPECHECK_UINT(x) if(x != 0b00100000)
#define LIMEADE_TYPECHECK_INT(x)  if(x != 0b01000000)
#define LIMEADE_TYPECHECK_FLT(x)  if(x != 0b01100000)
#define LIMEADE_TYPECHECK_ZERO(x) if(x != 0b00000000)
#define LIMEADE_SIZECHECK(x, y)   if(*(uint8_t*)x & 0b00011111 > y)
#define LIMEADE_CAST_FUNC(name, type, typecheck) \
static int name(void *src, void *dst)          \
{                                                \
  uint8_t datatype = *(uint8_t*)src & 0b11100000; \
  typecheck(datatype)                          \
  {                                              \
    if(datatype != 0b00000000)                   \
      return -1;                                 \
    else                                         \
    {                                            \
      *(type*)dst = 0;                         \
      return 1;                                  \
    }                                            \
  }                                              \
  LIMEADE_SIZECHECK(src, sizeof(type))         \
    return -1;                                   \
  *(type*)dst = *(type*)(src + 1);           \
  return 1 + sizeof(type);                     \
}

#endif /* _LIBLIMEADE_INTERNAL_H_ */
