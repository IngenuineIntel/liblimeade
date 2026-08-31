// parse_helpers.c
// AGPL


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

