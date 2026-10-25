// liblimeade-diag.c
// diagnostic functions for liblimeade
// AGPL

#ifndef _LIBLIMEADE_DIAG_C
#define _LIBLIMEADE_DIAG_C


#include<liblimeade/liblimeade.h>

void limeade_diag_error(enum limeade_error x);
void limeade_diag_indiv_recv(struct limeade_indiv_recv x);
void limeade_diag_recv_data(struct limeade_recv_data x);
void limeade_diag_context(struct limeade_context x);
void limeade_diag_packet(enum limeade_packet x);
void limeade_diag_packet_data(struct limeade_packet_data x);
void limeade_diag_packet_flags(struct limeade_packet_flags x);
void limeade_diag_introduction(struct limeade_intro x);
#define limeade_diag_intro limeade_diag_introduction
void limeade_diag_acknowledge(struct limeade_ack x);
#define limeade_diag_ack limeade_diag_acknowledge
void limeade_diag_indiv_event(struct limeade_indiv_event x);
void limeade_diag_events(struct limeade_events x);
void limeade_diag_indiv_proc(struct limeade_indiv_proc x);
void limeade_diag_proc_generic(struct limeade_proc_generic x);
void limeade_diag_proc_update(struct limeade_proc_update x);
void limeade_diag_perf(struct limeade_perf x);
void limeade_diag_close(struct limeade_close x);

// if a user supplies any sort of enum to limeade_diag, I'd rather give them
// this than give them nothing
void limeade_diag_enum(int x);

#endif /* _LIBLIMEADE_DIAG_C */

#ifdef limeade_diag
#undef limeade_diag
#endif

#ifdef ENABLE_LIBLIMEADE_DIAGNOSTICS

#define limeade_diag(x) _Generic((x),\
  struct limeade_indiv_recv:            limeade_diag_indiv_recv,           \
  struct limeade_recv_data:             limeade_diag_recv_data,            \
  struct limeade_context:               limeade_diag_context,              \
  struct limeade_packet_data:           limeade_diag_packet_data,          \
  struct limeade_packet_flags:          limeade_diag_packet_flags,         \
  struct limeade_intro:                 limeade_diag_intro,                \
  struct limeade_acknowledge:           limeade_diag_acknowledge,          \
  struct limeade_indiv_event:           limeade_diag_indiv_event,          \
  struct limeade_events:                limeade_diag_events,               \
  struct limeade_indiv_proc:            limeade_diag_indiv_proc,           \
  struct limeade_proc_generic:          limeade_diag_proc_generic,         \
  struct limeade_proc_update:           limeade_diag_proc_update,          \
  struct limeade_perf:                  limeade_diag_perf,                 \
  struct limeade_close:                 limeade_diag_close,                \
  int:                                  limeade_diag_enum,                 \
)(x)

#else

#define limeade_diag(x) do{} while(0)

#endif /* ENABLE_LIBLIMEADE_DIAGNOSTICS */
