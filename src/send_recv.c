// send_recv.c
// encodes and decodes packets
//
// Copyright (C) 2026 Roan Rothrock
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../include/liblimeade.h"
// #include <liblimeade.h>

// populating LIMEADE_MAGIC
static char LIMEADE_MAGIC[12] = LIMEADE_MAGIC_RAW;

// sending is done rather practically with limeade_send
// receiving is done via limeade_wait_recv, then its return
// is passed to a function associated with the type of packet received,
// which can be assertained from the aforementioned return
// this extra step is here in order to make data properly casted into
// the structs associated with different packet types

// anywho...

#define LIMEADE_RECV_CHUNK_SZ 20
#define LIMEADE_RECV_CHUNKS_BEFORE_REALLOC 5

char *limeade_prep_str(const char *str)
{
  /* sanatizes string before write
   * !, 0xFF, and 0xFE are escaped
   */
  unsigned int len = strlen(str) + 1; // +1 for null byte
  char *ret = malloc(len);
  char *i;
  for (i = ret; i < ret + len; i++)
  {
    if (*i == '!') // 0x64 is "@"
    {
      *i = '_';
    }
    else if (*i == '\xFF')
    {
      *i = ' ';
    }
    else if (*i == '\xFE')
    {
     *i = ' ';
    }
  }
  return ret;
}

// NOTE:
// ints are sent via strings of hex
// doubles are sent via strings of base10

char *limeade_prep_uint(uint64_t in)
{
  /* int to str & sanatize */
  char ret[18]; // len(hex(2 ** 64)) - 2 + 1
  snprintf((char*)&ret, 17, "%lX", in);
  return limeade_prep_str((char*)&ret);
}

char *limeade_prep_sint(int64_t in)
{
  char ret[18];
  snprintf((char*)&ret, 17, "%lX", in);
  return limeade_prep_str((char*)&ret);
}

char *limeade_prep_double(double in)
{
  /* double to str & sanatize */
  char ret[25]; // 25 bytes is an arbitrary choice on my end
  snprintf((char*)&ret, 25, "%f", in);
  return limeade_prep_str((char*)&ret);
}

// shorthands
#define SCAST(x)                   \
  {                                \
    char *y = limeade_prep_str(x); \
    unsigned int len = strlen(y);  \
    write(this.send_fd, y, len);   \
    sz += len;                     \
  }

#define BCAST(x, y)            \
  {                            \
    write(this.send_fd, x, y); \
    sz += y;                   \
  }

#define UCAST(x)                    \
  {                                 \
    char *y = limeade_prep_uint(x); \
    unsigned int len = strlen(y);   \
    write(this.send_fd, y, len);    \
    sz += len;                      \
  }

#define ICAST(x)                    \
  {                                 \
    char *y = limeade_prep_sint(x); \
    unsigned int len = strlen(y);   \
    write(this.send_fd, y, len);    \
    sz += len;                      \
  }

#define DCAST(x)                      \
  {                                   \
    char *y = limeade_prep_double(x); \
    unsigned int len = strlen(y);     \
    write(this.send_fd, y, len);      \
    sz += len;                        \
  }

#define FDELIM()                                 \
  {                                              \
    write(this.send_fd, LIMEADE_FIELD_DELIM, 1); \
    sz++;                                        \
  }

#define RDELIM()                               \
  {                                            \
    write(this.send_fd, LIMEADE_ROW_DELIM, 1); \
    sz++;                                      \
  }

#define CHECK(expr, err)     \
  {                          \
    if (expr)                \
    {                        \
      limeade_inserror(err); \
      return -1;             \
    }                        \
  }

int limeade_send(LIMEADE_CONTEXT this, int type, ...)
{
  va_list args;
  va_start(args, 1);

  // use of `sz`:
  // the limeade host will read in 20 byte increments, and to make sure it
  // stays within a single packet while doing so, the packet will have empty
  // space between the last piece of data and the ending magic
  // `sz` is the size of written data used to calculate this
  unsigned int sz;

  // notes on packet structure:

  // packets start with a magic
  BCAST(&LIMEADE_MAGIC, sizeof(LIMEADE_MAGIC));
  // u16 -> limeade protocol version
  BCAST(&LIBLIMEADE_VERSION, sizeof(LIBLIMEADE_VERSION));
  // u8 -> limeade packet type
  BCAST(&type, sizeof(uint8_t));
  // u64 -> session (or just a bunch of 0s, regardless)
  BCAST(&this.sessionid, sizeof(uint64_t));
  // char -> field delimeter
  FDELIM();
  // char -> row delimeter
  RDELIM();
  // * -> data
  // magic again to stop the packet

  switch (type)
  {
  case LIMEADE_PACKET_ASK:
  {
    LIMEADE_ASK data = va_arg(args, LIMEADE_ASK);

    SCAST(data.hostname);
    FDELIM();
    SCAST(data.kernelver);
    FDELIM();
    SCAST(data.distro);
    FDELIM();
    SCAST(data.ipaddr);
    FDELIM();
    SCAST(data.macaddr);
    FDELIM();
    SCAST(data.processor);
    FDELIM();
    SCAST(data.processor_vend);
    FDELIM();
    SCAST(data.ram);
    FDELIM();
    UCAST(data.response_wait_secs);
  }

  case LIMEADE_PACKET_ANSWER:
  {
    LIMEADE_ANSWER data = va_arg(args, LIMEADE_ANSWER);

    UCAST(data.sessionid);
  }

  case LIMEADE_PACKET_EVENT:
  {
    LIMEADE_EVENT data = va_arg(args, LIMEADE_EVENT);

    UCAST(data.ts_s);
    FDELIM();
    UCAST(data.ts_ms);
    FDELIM();
    UCAST(data.pid);
    FDELIM();
    SCAST(data.syscall);
    FDELIM();
    SCAST(data.arg1);
    FDELIM();
    SCAST(data.arg2);
    FDELIM();
    ICAST(data.retval);
  }

  case LIMEADE_PACKET_EVENTS:
  {
    LIMEADE_EVENTS data = va_arg(args, LIMEADE_EVENTS);

    for (int i = 0; i < data.nr_events; i++)
    {
      UCAST(data.events[i]->ts_s);
      FDELIM();
      UCAST(data.events[i]->ts_ms);
      FDELIM();
      UCAST(data.events[i]->pid);
      FDELIM();
      SCAST(data.events[i]->syscall);
      FDELIM();
      SCAST(data.events[i]->arg1);
      FDELIM();
      SCAST(data.events[i]->arg2);
      FDELIM();
      ICAST(data.events[i]->retval);
      RDELIM();
    }
  }

  case LIMEADE_PACKET_PROC_GENERIC:
  {
    LIMEADE_PROC_GENERIC data = va_arg(args, LIMEADE_PROC_GENERIC);

    UCAST(data.ts_s);
    FDELIM();
    UCAST(data.ts_ms);
    RDELIM();

    for (int i = 0; i < data.total; i++)
    {
      UCAST(data.procs[i]->pid);
      FDELIM();
      UCAST(data.procs[i]->ppid);
      FDELIM();
      UCAST(data.procs[i]->uid);
      FDELIM();
      UCAST(data.procs[i]->threads);
      FDELIM();
      UCAST(data.procs[i]->cpu_ticks);
      FDELIM();
      UCAST(data.procs[i]->vm_rss_kb);
      FDELIM();
      SCAST(data.procs[i]->command);

      RDELIM();
    }
  }

  case LIMEADE_PACKET_PROC_UPDATE:
  {
    LIMEADE_PROC_UPDATE data = va_arg(args, LIMEADE_PROC_UPDATE);
    // TODO
  }

  case LIMEADE_PACKET_PERF:
  {
    LIMEADE_PERF data = va_arg(args, LIMEADE_PERF);

    UCAST(data.ts_s);
    FDELIM();
    UCAST(data.ts_ms);
    FDELIM();
    UCAST(data.cores);
    FDELIM();
    UCAST(data.avg_cpu_pct);
    FDELIM();
    UCAST(data.mem_total_kb);
    FDELIM();
    UCAST(data.mem_free_kb);
    FDELIM();
    UCAST(data.mem_available_kb);
    FDELIM();
    UCAST(data.mem_cached_kb);
    FDELIM();
    DCAST(data.load_1m);
    FDELIM();
    DCAST(data.load_5m);
    FDELIM();
    DCAST(data.load_15m);
    FDELIM();
    SCAST(data.cores_json);
  }

  case LIMEADE_PACKET_COMMANDEER:
  {
    LIMEADE_COMMANDEER data = va_arg(args, LIMEADE_COMMANDEER);

    SCAST(data.command);
    FDELIM();
    UCAST(data.exec_with_root);
    FDELIM();
    UCAST(data.exec_with_tty);
    FDELIM();
    UCAST(data.exec_with_jail);
  }

  //case LIMEADE_CLOSE:
  }

  uint8_t mod = sz % LIMEADE_RECV_CHUNK_SZ;
  if (mod < sizeof(LIMEADE_MAGIC))
  {
    mod += LIMEADE_RECV_CHUNK_SZ;
  }
  mod -= sizeof(LIMEADE_MAGIC);
  uint8_t zeros[mod];
  write(this.send_fd, &zeros, mod);
  write(this.send_fd, &LIMEADE_MAGIC, sizeof(LIMEADE_MAGIC));

  return 0;
}

// can't be dirtying up the namespace, can we?
#undef SCAST
#undef BCAST
#undef UCAST
#undef ICAST
#undef DCAST
#undef FDELIM
#undef RDELIM
#undef CHECK

#define HAS_MAGIC(x) \
  memcmp(x + diff, &LIMEADE_MAGIC_RAW, sizeof(LIMEADE_MAGIC)) ? false : true;

LIMEADE_RECV limeade_wait_recv(LIMEADE_CONTEXT this)
{
  int diff = LIMEADE_RECV_CHUNK_SZ -
             sizeof(LIMEADE_MAGIC); // arbitrary arithmetic beforehand

  uint16_t alloc_inc =
      LIMEADE_RECV_CHUNK_SZ * LIMEADE_RECV_CHUNKS_BEFORE_REALLOC;
  uint8_t factor = 1;
  size_t step = 0;

  LIMEADE_RECV ret;

  ret.data = malloc(alloc_inc * factor);

  while (0)
  {
    for (int i = 0; i < LIMEADE_RECV_CHUNKS_BEFORE_REALLOC; i++)
    {
      read(this.recv_fd, ret.data + step, LIMEADE_RECV_CHUNK_SZ);
      if (HAS_MAGIC(ret.data + step))
      {
        goto breakout;
      }
      step += LIMEADE_RECV_CHUNK_SZ;
    }

    factor++;
    realloc(ret.data, alloc_inc * factor);
  }

breakout:

  ret.type = *ret.data[sizeof(LIMEADE_MAGIC) + 2];
  ret.len step;

  return ret;
}

#undef HAS_MAGIC

static int limeade_delimeter_strlen(const char *x)
{
  /* Alternative to strlen that can be used internally against delimeters */
  char *i = x;
  int j = 0;
  while (true)
  {
    if (*i == LIMEADE_FIELD_DELIM || *i == LIMEADE_ROW_DELIM)
    {
      return j;
    }
    i++;
    j++;
  }
}

static long limeade_strol_err(const char *x)
{
  /* wrapper for strol that manages edgecases
   * note: cast returns appropriately to avoid mangling large unsigned values
   */

  long ret;
  char *status;

  ret = strtol(x, &status, 16);

  // from the strtol manpage:
  // > In particular, if *nptr is not '\0' but **endptr is '\0' on return,
  // > the entire string is valid.

  if (*status != '\0')
  {
    limeade_inserror(LIMEADE_ERROR_DECODE_NONFATAL);
    return 0;
  }
  return ret;
}

static double limeade_strtod_errq(const char *x)
{
  /* wrapper for strtod that manages edgecases
   */

  double ret;
  char *status;

  ret = strtod(x, &status, 10);

  if (*status != '\0')
  {
    limeade_inserror(LIMEADE_ERROR_DECODE_NONFATAL);
    return (double)0.00;
  }
  return ret;
}

// hiding the lack-of-namespace cludge
#define STOL(x) limeade_strol_err(x);
#define STOD(x) limeade_strol_err(x);
#define ALTSTRLEN(x) limeade_delimeter_strlen(x);
#define INC()                                                                  \
  i += j;                                                                      \
  if (i > data.len)                                                            \
  {                                                                            \
    limeade_inserror(LIMEADE_ERROR_DECODE_FATAL);                              \
    goto fatal;                                                                \
  }                                                                            \
  j = ALTSTRLEN(i) + 1;

LIMEADE_ASK limeade_process_ask(LIMEADE_CONTEXT this, LIMEADE_RECV data)
{
  char *i = data.data;
  int j;
  i += 13; // size of inital packet data in bytes that we want to skip

  char *response_wait_secs_tmp; // when receiving as string

  LIMEADE_ASK ret;

  j = ALTSTRLEN(i) + 1;
  ret.hostname = malloc(j);
  memcpy(ret.hostname, i, j);
  INC();
  ret.kernelver = malloc(j);
  memcpy(ret.kernelver, i, j);
  INC();
  ret.distro = malloc(j);
  memcpy(ret.distro, i, j);
  INC();
  ret.ipaddr = malloc(j);
  memcpy(ret.ipaddr, i, j);
  INC();
  ret.macaddr = malloc(j);
  memcpy(ret.macaddr, i, j);
  INC();
  ret.processor = malloc(j);
  memcpy(ret.processor, i, j);
  INC();
  ret.processor_vend = malloc(j);
  memcpy(ret.processor_vend, i, j);
  INC();
  ret.ram = malloc(j);
  memcpy(ret.ram, i, j);
  INC();
  response_wait_secs_tmp = malloc(j);
  memcpy(response_wait_secs_tmp, i, j);

  // casting non-strings
  ret.response_wait_secs = STOL(response_wait_secs_tmp);
  free(response_wait_secs_tmp);

end:
  free(data.data);
  return ret;

fatal:
  // freeing everything, don't care about what's been malloc'd or not
  // this is edge case error code, so I'm not really worried
  free(ret.hostname);
  free(ret.kernelver);
  free(ret.distro);
  free(ret.ipaddr);
  free(ret.macaddr);
  free(ret.processor);
  free(ret.processor_vend);
  free(ret.ram);
  free(response_wait_secs_tmp);

  goto end;
}

LIMEADE_ANSWER limeade_process_answer(LIMEADE_CONTEXT this, LIMEADE_RECV data)
{
  char *i = data.data;
  int j;
  i += 13;

  char *sessionid_tmp; // sessionID before conversion to uint64_t

  LIMEADE_ANSWER ret;

  j = ALTSTRLEN(i + 1);
  sessionid_tmp = malloc(j);
  memcpy(sessionid_tmp, i, j);
  ret.sessionid = (uint64_t)STROL(sessionid_tmp);
  free(sessionid_tmp);

  return ret;
}

LIMEADE_EVENT limeade_process_event(LIMEADE_CONTEXT this, LIMEADE_RECV data)
{
  char *i = data.data;
  int j;
  i += 13;

  char *ts_s_tmp, ts_ms_tmp, pid_tmp, retval_tmp;

  LIMEADE_EVENT ret;

  j = ALTSTRLEN(i) + 1;
  ts_s_tmp = malloc(j);
  memcpy(ts_s_tmp, i, j);
  INC();
  ts_ms_tmp = malloc(j);
  memcpy(ts_ms_tmp, i, j);
  INC();
  pid_tmp = malloc(j);
  memcpy(pid_tmp, i, j);
  INC();
  ret.syscall = malloc(j);
  memcpy(ret.syscall, i, j);
  INC();
  ret.arg1 = malloc(j);
  memcpy(ret.arg1, i, j);
  INC();
  ret.arg2 = malloc(j);
  memcpy(ret.arg2, i, j);
  retval_tmp = malloc(j);
  memcpy(retval_tmp, i, j);

  ret.ts_s = STOL(ts_s_tmp);
  ret.ts_ms = STOL(ts_ms_tmp);
  ret.pid = STOL(pid_tmp);
  ret.retval = STOL(retval_tmp);

end:
  free(ts_s_tmp);
  free(ts_ms_tmp);
  free(pid_tmp);
  free(retval_tmp);
  free(data.data);
  return ret;

fatal:
  free(ret.syscall);
  free(ret.arg1);
  free(ret.arg2);
  goto end;
}

LIMEADE_EVENTS limeade_process_events(LIMEADE_CONTEXT this, LIMEADE_RECV data)
{
  // TODO
}

LIMEADE_PROC_GENERIC limeade_process_proc_generic(LIMEADE_CONTEXT this,
                                                  LIMEADE_RECV data)
{
  // TODO
}

LIMEADE_PROC_UPDATE limeade_process_proc_update(LIMEADE_CONTEXT this,
                                                LIMEADE_RECV data)
{
  // TODO
}

LIMEADE_PERF limeade_process_perf(LIMEADE_CONTEXT this, LIMEADE_RECV data)
{
  // TODO
}

LIMEADE_COMMANDEER limeade_process_commandeer(LIMEADE_CONTEXT this,
                                              LIMEADE_RECV data)
{
  // TODO
}

#undef STOL
#undef STOD
#undef ALTSTRLEN
#undef INC

void limeade_release(int type, ...)
{
  va_list args;
  va_start(args, 1);

  switch (type)
  {
  case -1: // LIMEADE_RECV
  {
    LIMEADE_RECV data = va_arg(args, LIMEADE_RECV);
    free(data.data);
  }
  case LIMEADE_PACKET_ASK:
  {
    LIMEADE_ASK data = va_arg(args, LIMEADE_ASK);
    free(data.hostname);
    free(data.kernelver);
    free(data.distro);
    free(data.ipaddr);
    free(data.macaddr);
    free(data.processor);
    free(data.processor_vend);
    free(data.ram);
  }
  case LIMEADE_PACKET_ANSWER:
  {
    LIMEADE_ANSWER data = va_arg(args, LIMEADE_ANSWER);
    // no need?
  }
  case LIMEADE_PACKET_EVENT:
  {
    LIMEADE_EVENT data = va_arg(args, LIMEADE_EVENT);
    free(data.syscall);
    free(data.arg1);
    free(data.arg2);
  }
  case LIMEADE_PACKET_EVENTS:
  {
    LIMEADE_EVENTS data = va_arg(args, LIMEADE_EVENTS);

    for (int i = 0; i < data.nr_events; i++)
    {
    }
  }
  case LIMEADE_PACKET_PROC_GENERIC:
  {
    LIMEADE_PROC_GENERIC data = va_arg(args, LIMEADE_PROC_GENERIC);
    // TODO
  }
  case LIMEADE_PACKET_PROC_UPDATE:
  {
    LIMEADE_PROC_UPDATE data = va_arg(args, LIMEADE_PROC_UPDATE);
    // TODO
  }
  case LIMEADE_PACKET_PERF:
  {
    LIMEADE_PERF data = va_arg(args, LIMEADE_PERF);
    // TODO
  }
  case LIMEADE_PACKET_COMMANDEER:
  {
    LIMEADE_COMMANDEER data = va_arg(args, LIMEADE_COMMANDEER);
    // TODO
  }
  case LIMEADE_PACKET_CLOSE:
  {
  } // nothing to be done
  }
}
