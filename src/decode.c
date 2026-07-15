// decode.c
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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <zlib.h>

#include <liblimeade.h>

TS limeade_gents(void)
{
  /* generates timestamp data for this exact moment */
  TS ret;
  struct timespec tm;

  if (!clock_gettime(CLOCK_REALTIME, &tm))
  {
    return ret;
  }

  ret.s  = (uint32_t)tm.tv_sec;
  ret.ms = (uint16_t)(tm.tv_nsec / 1000000); // ns -> ms

  return ret;
}

LIMEADE_PARSED limeade_parse_packet(LIMEADE_PACKET pkt)
{
  LIMEADE_PARSED ret = {0};
  const size_t magic_len = sizeof(LIMEADE_MAGIC);
  const size_t flags_len = 7;
  const size_t session_len_wire = 5;
  size_t session_len;
  size_t payload_len;
  size_t expected_len;
  const uint8_t *payload;

  ret.when_parsed = limeade_gents();

  if (pkt.data == NULL || pkt.sz < (uint32_t)(magic_len + flags_len))
  {
    limeade_inserror(LIMEADE_ERROR_PACKET_FORMAT);
    return ret;
  }

  {
    uint64_t flags = 0;
    memcpy(&flags, wire + magic_len, 7);

    ret.flags.version                   = (LIBLIMEADE_VERSION)(flags & 0xFF);
    ret.flags.type                      = (LIMEADE_PACKET_TYPE)((flags >> 12) & 0x0F);
    ret.flags.datasz_before_compression = (uint16_t)(((flags >>  8) & 0x0F) << 10
                                                   | ((flags >> 16) & 0xFF) <<  2
                                                   | ((flags >> 30) & 0x03));
    ret.flags.datasz_after_compression  = (uint16_t)(((flags >> 24) & 0x3F) <<  8
                                                   | ((flags >> 32) & 0xFF));
    ret.flags.field_delim               = (char)((flags >> 40) & 0xFF);
    ret.flags.row_delim                 = (char)((flags >> 48) & 0xFF);

    if (ret.flags.datasz_before_compression == 0 && ret.flags.datasz_after_compression > 0)
    {
      limeade_inserror(LIMEADE_ERROR_PACKET_FORMAT);
      return ret;
    }
  }

  session_len = (ret.flags.type == LIMEADE_CLIENT_ASK) ? 0 : session_len_wire;
  payload_len = (ret.flags.datasz_after_compression > 0)
      ? (size_t)ret.flags.datasz_after_compression
      : (size_t)ret.flags.datasz_before_compression;

  expected_len = magic_len + flags_len + session_len + payload_len;
  if ((size_t)pkt.sz != expected_len)
  {
    limeade_inserror(LIMEADE_ERROR_PACKET_SIZE);
    return ret;
  }

  if (session_len > 0)
  {
    memcpy(ret.sessionid, wire + magic_len + flags_len, session_len_wire);
  }
  else
  {
    memset(ret.sessionid, 0, session_len_wire);
  }

  payload = wire + magic_len + flags_len + session_len;

  if (ret.flags.datasz_after_compression > 0)
  {
    uLongf out_len = (uLongf)ret.flags.datasz_before_compression;

    ret.data = malloc((size_t)ret.flags.datasz_before_compression);
    if (ret.data == NULL)
    {
      limeade_inserror(LIMEADE_ERROR_PACKET_SIZE);
      return ret;
    }

    if (uncompress((Bytef *)ret.data, &out_len, (const Bytef *)payload,
                   (uLong)ret.flags.datasz_after_compression) != Z_OK
        || out_len != (uLongf)ret.flags.datasz_before_compression)
    {
      free(ret.data);
      ret.data = NULL;
      limeade_inserror(LIMEADE_ERROR_PACKET_FORMAT);
      return ret;
    }
  }
  else if (payload_len > 0)
  {
    ret.data = malloc(payload_len);
    if (ret.data == NULL)
    {
      limeade_inserror(LIMEADE_ERROR_PACKET_SIZE);
      return ret;
    }

    memcpy(ret.data, payload, payload_len);
  }

  limeade_inserror(LIMEADE_SUCCESS);
  return ret;
}
