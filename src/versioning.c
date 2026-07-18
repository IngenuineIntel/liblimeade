// versioning.c

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

// liblimeade v0.1
LIBLIMEADE_VERSION LIMEADE_PROTOCOL_VERSION = {0, 1};

LIBLIMEADE_COMPATIBILITY
liblimeade_check_versioning(LIBLIMEADE_VERSION local_ver,
                            LIBLIMEADE_VERSION target_ver)
{
  int diff = local_ver.maj - target_ver.maj;

  // diff = abs(diff)
  if (diff < 0)
  {
    diff = -diff;
  }

  int is_ident_maj = (diff == 0) ? 1 : 0;
  int is_ident_min = (local_ver.min == target_ver.min) ? 1 : 0;

  // same major version
  if (is_ident_maj)
  {
    // versions have to be identical when before v1.0, otherwise indeterminate
    if (local_ver.maj == 0 && !is_ident_min)
      return INDETERMINATE_COMPATIBILITY;

    return IS_COMPATIBLE;
  }
  else if (diff == 1)
  {
    // versions are within 1 major version
    // insert exceptions here
    return IS_COMPATIBLE;
  }
  return IS_NOT_COMPATIBLE;
}
