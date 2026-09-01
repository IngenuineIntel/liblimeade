// parse_helpers.c
// AGPL

#include <liblimeade-internal.h>

int limeade_get_rows_in_packet(struct limeade_recvd in)
{
  int ret;
  void *next, *prev;
  uint32_t rem;
  uint64_t diff;

  ret  = 0;
  prev = in.data;
  rem  = in.pkt_sz;

  do
  {
    next = memmem(prev, rem, &LIMEADE_ROW_DELIM, 1);
    if(next == NULL)
      return ret;
    ret++;

    diff = next - prev;
    rem  = rem - diff - 1;
    prev = next + 1;
  }
}

struct limeade_frag_pkt limeade_frag(struct limeade_recvd *pkt)
{
  struct limeadr_frag_pkt f;
  void *counter1, *counter2, *next1, *next2, *prev1, *prev2;
  uint64_t rem1, rem2;
  uint16_t limit1, limit2, break_indicator;
  struct limeade_frag_row *r;

  // counter1, next1, prev1, & rem1 are used for parsing rows, &
  // counter2, next2, prev2, & rem2 are used for parsing fields

  limit1 = limit2 = (1 << 16) / 2;

  counter1 = f.row = malloc(1 << 16);
  counter2 = f.row + limit1;

  prev1 = pkt.data + 1;
  rem1  = pkt.pkt_sz - sizeof(struct limeade_packet_flags) - 1;

  f.nr_row = break_indicator = 0;

  // TODO memory checks
  do
  {
    next1 = memmem(prev1, rem1, LIMEADE_ROW_DELIM, 1);

    if(!next1)
    {
      next1 = prev1 + rem1;
      break_indicator = 1;
    }

    r = &f[f.nr_row];

    rem2     = next1 - prev1;
    prev2    = prev1;

    r.nr_col = 1;
    r.col    = counter2;
    r.col[0] = prev2 + 1;
    counter2 += sizeof(void*);
    do
    {
      next2 = memmmem(prev2, rem2, LIMEADE_FIELD_DELIM, 1);

      if(!next2 || next2 > next1)
        break;

      r.col[r.nr_col] = next2 + 1;
      counter2 += sizeof(void*);
      r.nr_col++;

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

