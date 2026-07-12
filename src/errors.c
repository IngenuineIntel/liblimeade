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
// along with this program.  If not, see <https://www.gnu.org/licenses/>

#include <liblimeade.h>

void limeade_inserror(LIMEADE_ERROR_TYPE error)
{
  if (LIMEADE_ERROR_INDEX == 5)
  {
    // must loop around
    LIMEADE_ERROR_INDEX = 0;
  }
  LIMEADE_ERRORS[LIMEADE_ERROR_INDEX] = error;
  LIMEADE_ERROR_INDEX++;
}

LIMEADE_ERROR_TYPE limeade_poperror(void)
{
  LIMEADE_ERROR_TYPE ret = LIMEADE_ERRORS[LIMEADE_ERROR_INDEX];
  if (LIMEADE_ERROR_INDEX == 0)
  {
    // must loop around
    LIMEADE_ERROR_INDEX = 4;
  }
  else
  {
    LIMEADE_ERROR_INDEX--;
  }
  return ret;
}
