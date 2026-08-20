// monotonic.c
//

#include<time.h>

void limeade_monotonic(struct timespec *ts)
{
  if(clock_gettime(CLOCK_MONOTONIC, ts) != 0)
  {
    limeade_inserr(LIMEADE_ERROR_MONOTONIC);
  } else
  {
    limeade_inserr(LIMEADE_SUCCESS);
  }
}
