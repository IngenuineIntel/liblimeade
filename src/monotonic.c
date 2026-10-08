// monotonic.c
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

#include<time.h>

#include<liblimeade/liblimeade-internal.h>

int limeade_monotonic(struct timespec *ts)
{
  /* monotonic timestamp wrapper */
  if(clock_gettime(CLOCK_MONOTONIC, ts) != 0)
    return LIMEADE_ERROR_MONOTONIC;
  return LIMEADE_SUCCESS;
}

int64_t limeade_monotonic_diff_ms(struct timespec *a, struct timespec *b)
{
  /* calculates the difference between two time values in milliseconds */
  int64_t a_t, b_t, ret;

  a_t = (a->tv_sec * 1000) + (a->tv_nsec / 1000000);
  b_t = (b->tv_sec * 1000) + (b->tv_nsec / 1000000);

  ret = b_t - a_t;

  return ret > 0 ? ret : 0 - ret;
  
}
