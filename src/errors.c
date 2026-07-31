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



void limeade_inserror(LIMEADE_ERROR error)
{
  LIMEADE_ERRORS[LIMEADE_ERROR_INDEX] = error;

  if(LIMEADE_ERROR_INDEX == LIMEADE_ERROR_BUFFER_SIZE - 1)
  {
    LIMEADE_ERROR_INDEX = 0;
  } else {
    LIMEADE_ERROR_INDEX++;
  }
}

LIMEADE_ERROR limeade_poperror(void)
{
  if (LIMEADE_ERROR_INDEX == 0)
  {
    return LIMEADE_ERRORS[LIMEADE_ERROR_BUFFER_SIZE - 1];
  } else
  {
    return LIMEADE_ERRORS[LIMEADE_ERROR_INDEX - 1];
  }
}
