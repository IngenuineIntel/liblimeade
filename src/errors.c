// errors.c
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
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include <liblimeade/liblimeade.h>

// FIXME with a mutex!!

// the buffer itself
LIMEADE_ERROR limeade_errors[LIMEADE_ERROR_BUFFER_SIZE];

// manages which index in the buffer is the latest error
// if =1, the latest error is at [1], the previous [0], and the next previous
// [5], etc.
unsigned int limeade_error_index;



void limeade_inserror(LIMEADE_ERROR error)
{
  limeade_errors[limeade_error_index] = error;

  if(limeade_error_index == LIMEADE_ERROR_BUFFER_SIZE - 1)
  {
    limeade_error_index = 0;
  } else {
    limeade_error_index++;
  }
}

LIMEADE_ERROR limeade_poperror(void)
{
  if (limeade_error_index == 0)
  {
    limeade_error_index = LIMEADE_ERROR_BUFFER_SIZE - 1;
  } else
  {
    limeade_error_index --;
  }

  limeade_errors[limeade_error_index] = LIMEADE_BLANK_ERROR;
  return limeade_errors[limeade_error_index];
}

void *limeade_get_errors(void)
{
  /* wrapper to access the buffer externally */
  return &limeade_errors;
}

unsigned int limeade_get_error_index(void)
{
  /* wrapper to access the index externally */
  return limeade_error_index;
}
