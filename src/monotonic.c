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

int64_t limeade_monotonic_diff_ms(struct timespec *a, struct timespec *b)
{
  int64_t ret, ms;

  ret = b->tv_sec - a->tv_sec;

  // in theory, b should be the latter timestamp, but just in case:
  if(ret < 0)
  {
    ret = 0 - ret;
  }

  ret *= 1000;

  ms = (t->tv_nsec - a->tv_nsec) / 1000000;

  if(ms < 0)
  {
    ms = 0 - ms;
  }
  ret += ms;
  return ret;
}

