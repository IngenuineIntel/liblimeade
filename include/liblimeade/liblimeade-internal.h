// liblimeade-internal.h
// internal functions for liblimeade
// AGPL

#ifndef _LIBLIMEADE_INTERNAL_H_
#define _LIBLIMEADE_INTERNAL_H_

#include<liblimeade/liblimeade.h>

/* limeade_monotonic
 *
 * wrapper for monotonic timestamps
 */
void limeade_monotonic(struct timespec *ts);

/* limeade_monotonic_diff_ms
 *
 * gets milliseconds between `a` & `b`
 */
void limeade_monotonic_diff_ms(struct timespec *a, struct timespec *b);

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

#endif /* _LIBLIMEADE_INTERNAL_H_ */
