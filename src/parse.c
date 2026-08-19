// parse.c
// receiving and parsing
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

#include<pthread.h>
#include<stdlib.h>
#include<string.h>

#include<liblimeade/liblimeade.h>

struct limeade_recv limeade_recv(struct limeade_context *ctx)
{
}

struct limeade_packet_flags limeade_parse_flags(struct limeade_recv data)
{
  struct limeade_packet_flags ret;
  int len = memcpy(&ret, data.flags, sizeof(limeade_packet_flags));
  if(len != sizeof(limeade_packet_flags))
  {
    limeade_inserr(LIMEADE_ERROR_BAD_DATA);
  } else
  {
    limeade_inserr(LIMEADE_SUCCESS);
  }
  return ret;
}

struct limeade_knock limeade_parse_knock(struct limeade_recv pkt)
{
  if(pkt.type != LIMEADE_PACKET_KNOCK)
  {
    limeade_inserr(LIMEADE_ERROR_GARBAGE);
    return (struct limeade_knock)NULL;
  }
}

struct limeade_recognize limeade_parse_recognize(struct limeade_recv pkt)
{
  if(pkt.type != LIMEADE_PACKET_RECOGNIZE)
  {
    return (struct limeade_recognize)NULL;
  }
}
struct limeade_intro limeade_parse_intro(struct limeade_recv pkt)
{
  if(pkt.type != LIMEADE_PACKET_INTRO)
  {
    return (struct limeade_intro)NULL;
  }
}
struct limeade_ack limeade_parse_ack(struct limeade_recv pkt)
{
  if(pkt.type != LIMEADE_PACKET_ACK)
  {
    limeade_inserr(LIMEADE_ERROR_GARBAGE);
    return (struct limeade_knock)NULL;
  }
}
struct limeae_events limeade_parse_events(struct limeade_recv pkt)
{
  if(pkt.type != LIMEADE_PACKET_EVENTS)
  {
    limeade_inserr(LIMEADE_ERROR_GARBAGE);
    return (struct limeade_knock)NULL;
  }
}
struct limeade_proc_generic limeade_parse_proc_generic(struct limeade_recv pkt)
{
  if(pkt.type != LIMEADE_PACKET_PROC_GENERIC)
  {
    limeade_inserr(LIMEADE_ERROR_GARBAGE);
    return (struct limeade_knock)NULL;
  }
}
struct limeade_proc_update limeade_parse_proc_update(struct limeade_recv pkt)
{
  if(pkt.type != LIMEADE_PACKET_PROC_UPDATE)
  {
    limeade_inserr(LIMEADE_ERROR_GARBAGE);
    return (struct limeade_knock)NULL;
  }
}
struct limeade_perf limeade_parse_perf(struct limeade_recv pkt)
{
  if(pkt.type != LIMEADE_PACKET_PERF)
  {
    limeade_inserr(LIMEADE_ERROR_GARBAGE);
    return (struct limeade_knock)NULL;
  }
}
struct limeade_commandeer limeade_parse_commandeer(struct limeade_recv pkt)
{
  if(pkt.type != LIMEADE_PACKET_COMMANDEER)
  {
    limeade_inserr(LIMEADE_ERROR_GARBAGE);
    return (struct limeade_knock)NULL;
  }
}
struct limeade_exited limeade_parse_exited(struct limeade_recv pkt)
{
  if(pkt.type != LIMEADE_PACKET_EXITED)
  {
    limeade_inserr(LIMEADE_ERROR_GARBAGE);
    return (struct limeade_knock)NULL;
  }
}
struct limeade_close limeade_parse_close(struct limeade_recv pkt)
{
  if(pkt.type != LIMEADE_PACKET_CLOSE)
  {
    limeade_inserr(LIMEADE_ERROR_GARBAGE);
    return (struct limeade_knock)NULL;
  }
}
