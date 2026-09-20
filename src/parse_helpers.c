// parse_helpers.c
// AGPL

#include<stdlib.h>
#include<string.h>

#include <liblimeade/liblimeade-internal.h>

struct limeade_frag_pkt limeade_frag(struct limeade_recvd *pkt)
{
  struct limeade_frag_pkt f;
  void *counter1, *counter2, *next1, *next2, *prev1, *prev2;
  uint64_t rem1, rem2;
  uint16_t limit1, limit2, break_indicator;
  struct limeade_frag_row *r;

  // counter1, next1, prev1, & rem1 are used for parsing rows, &
  // counter2, next2, prev2, & rem2 are used for parsing fields

  counter1 = f.row = malloc(LIMEADE_FRAG_TOTAL_ALLOCATION);
  counter2 = f.row + LIMEADE_FRAG_COL_START;

  prev1 = pkt->data + 1;
  rem1  = pkt->pkt_sz - sizeof(struct limeade_packet_flags) - 1;

  f.nr_row = break_indicator = 0;

  // TODO memory limit checks
  for(;;)
  {
    next1 = memmem(prev1, rem1, (void*)LIMEADE_ROW_DELIM, 1);

    if(!next1)
    {
      next1 = prev1 + rem1;
      break_indicator = 1;
    }

    r = &(f.row[f.nr_row]);

    rem2     = next1 - prev1;
    prev2    = prev1;

    r->nr_col = 1;
    r->col    = counter2;
    r->col[0] = prev2 + 1;
    counter2 += sizeof(void*);

    for(;;)
    {
      next2 = memmem(prev2, rem2, (void*)LIMEADE_FIELD_DELIM, 1);

      if(!next2 || next2 > next1)
        break;

      r->col[r->nr_col] = next2 + 1;
      counter2 += sizeof(void*);
      r->nr_col++;

      prev2 = next2;
      rem2 = next1 - next2;
    }

    if(break_indicator)
      break;

    f.nr_row++;

    counter1 += sizeof(*r);
    rem1 -= (next1 - prev1) + 1;
    prev1 = next1 + 1;
  }

  return f;

  err:

  f.nr_row = 0;
  free(f.row);
  return f;
}

inline void limeade_release_frag(struct limeade_frag_pkt pkt)
{
  free(pkt.row);
}

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

