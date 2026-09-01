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

struct limeade_fragged_row
{
  uint16_t nr; // number of the row
  void **; //
}

struct limeade_fragged_pkt
{
  uint16_t nr_cols;
  uint16_t *nr_rows; // uint16_t nr_rows[nr_columns];
  void **cols;       // void *cols[nr_cols]
  void *data;        // start of contiguous data that *cols[] utilizes
  // TODO excuse my 2D pointer array
}

// the following structures represent a datatype used internally to parse
// packets. It is, more or less, a 2D array of pointers to delimeters in
// the packet, but has the following caveats:
// 1. columns aren't fixed-width
// 2. the datatype exists within a single buffer

struct limeade_frag_row
{
  uint8_t nr_col;
  void **col;
};

struct limeade_frag_pkt
{
  uint16_t nr_row;
  struct limeade_frag_row *row;
};

// retreives the pointer to data stored at coordinate [x, y]
#define LIMEADE_FRAG_COOR(f, x, y) f.row[y].col[x]

/* limeade_frag
 *
 * processes packet data & returns a grid datatype of pointers to data within
 * the packet
 */
struct limeade_frag_pkt limeade_frag(struct limeade_recvd *pkt);

/* limeade_release_frag
 *
 * releases data returned by limeade_frag
 */
inline void limeade_release_frag(struct limeade_frag_pkt);

// the following macros are for creating functions used for parsing data from
// packets
#define LIMEADE_TYPECHECK_UINT(x) if(x != 0b00100000)
#define LIMEADE_TYPECHECK_INT(x)  if(x != 0b01000000)
#define LIMEADE_TYPECHECK_FLT(x)  if(x != 0b01100000)
#define LIMEADE_CAST_FUNC(name, type, typecheck)     \
static int name(void *src, type *dst, uint16_t rem)  \
{                                                    \
  register uint8_t databyte = *(uint8_t*)src;        \
  register uint8_t datatype = databyte & 0b11100000; \
  register uint8_t datasize = databyte & 0b00011111; \
  if(datasize > rem) return 0;                       \
  typecheck(datatype)                                \
  {                                                  \
    if(data_byte != 0b00000000 || datasize > y)      \
      return -1;                                     \
    else                                             \
    {                                                \
      *dst = 0;                                      \
      return 1;                                      \
    }                                                \
  }                                                  \
  *dst = *(type*)(src + 1);                          \
  return 1 + sizeof(type);                           \
}

#endif /* _LIBLIMEADE_INTERNAL_H_ */
