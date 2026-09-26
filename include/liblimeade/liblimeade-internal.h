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

// function generator for writing data into packets before sending
#define LIMEADE_UINT_INDICATOR 0b00100000
#define LIMEADE_SINT_INDICATOR 0b01000000
#define LIMEADE_FLT_INDICATOR 0b01100000
#define LIMEADE_GEN_PREP_FN(name, type, indicator)\
static int name(type in, void *out, int max)\
{\
  if(!out) return 0;\
  if(sizeof(in) + 1 > max) return -1;\
  if(in == 0)\
  {\
    *(uint8_t*)out = '\x00';\
    return 1;\
  }\
  *(uint8_t*)out = indicator | sizeof(in);\
  *((type*)((uint8_t*)out + 1)) = in;\
  return sizeof(in) + 1;\
}

/* limeade_statecheck
 *
 * checks any/all active threads and makes sure everything is in a functional
 * state
 *
 * returns LIMEADE_SUCCESS on success
 */
int limeade_statecheck(struct limeade_context *ctx);

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

// the following macros are for creating functions used for parsing data from
// packets
#define LIMEADE_TYPECHECK_UINT(x) if(x != 0b00100000)
#define LIMEADE_TYPECHECK_INT(x)  if(x != 0b01000000)
#define LIMEADE_TYPECHECK_FLT(x)  if(x != 0b01100000)
#define LIMEADE_CAST_FUNC(name, type, typecheck)     \
static int name(void *src, type *dst, int rem)       \
{                                                    \
  register uint8_t databyte = *(uint8_t*)src;        \
  register uint8_t datatype = databyte & 0b11110000; \
  register uint8_t datasize = databyte & 0b00011111; \
  if(datasize + 1 > rem) return 0;                   \
  typecheck(datatype)                                \
  {                                                  \
    if(databyte != 0b00000000)                       \
      return -1;                                     \
    else                                             \
    {                                                \
      *(type*)dst = 0;                               \
      return 2;                                      \
    }                                                \
  }                                                  \
  *dst = *(type*)(src + 1);                          \
  return 2 + sizeof(type);                           \
}

/* limeade_decompress_packet
 *
 * decompresses data section of packet
 *
 * 0 on success, -1 on error
 */
int limeade_decompress_packet(struct limeade_recvd pkt);

/* limeade_get_nr_rows
 *
 * self-explanatory
 */
int limeade_get_nr_rows(struct limeade_recvd *pkt);

/* limeade_pkt_strlen
 *
 * reimplementation of strlen that uses LIMEADE_FIELD_DELIM or LIMEADE_ROW_DELIM
 * instead of 0x00
 */
uint32_t limeade_pkt_strlen(const char *s, uint32_t max_len);

/* limeade_th_recv_wr_pkt
 *
 * takes a verified packet and places it into the context-wide
 * packet queue
 */
void limeade_th_recv_wr_pkt(struct limeade_recv_data *r, void *pkt,
                            unsigned int sz, struct sockaddr_in *addr,
                            socklen_t len);

/* limeade_eth_recv
 *
 * receives next UDP packet for UDP mode
 */
int limeade_eth_recv(const struct limeade_context *ctx,
                            const void *buffer, const unsigned int sz,
                            struct sockaddr *cliaddr, socklen_t *cli_len);

/* limeade_ssh_recv
 *
 * receives next SSH chunk for SSH mode
 */
inline int limeade_ssh_recv(const struct limeade_context *ctx,
                            const void *buffer, const unsigned int sz);

/* limeade_prelim_confirm
 *
 * does preliminary checks on a packet to make sure it is valid
 */
int limeade_prelim_confirm(const void *buffer, const int sz);

/*** th_recv functions ***/
void *limeade_th_recv_client_eth(void *arg);
void *limeade_th_recv_host_eth(void *arg);
void *limeade_th_recv_client_ssh(void *arg);
void *limeade_th_recv_client_libssh(void *arg);
void *limeade_th_recv_host_ssh(void *arg);

/*** th_csm ***/
void *limeade_th_csm(void *arg);

#endif /* _LIBLIMEADE_INTERNAL_H_ */
