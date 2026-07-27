// send_recv.c
// encodes and decodes packets
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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <liblimeade/liblimeade.h>

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
    if (*i == '@')
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
  va_start(args, type);

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
  memcmp(x + diff, &LIMEADE_MAGIC, sizeof(LIMEADE_MAGIC))

LIMEADE_RECV limeade_wait_recv(LIMEADE_CONTEXT this)
{
  int diff = LIMEADE_RECV_CHUNK_SZ -
             sizeof(LIMEADE_MAGIC); // arbitrary arithmetic beforehand

  uint16_t alloc_inc = LIMEADE_RECV_CHUNK_SZ * LIMEADE_RECV_CHUNKS_BEFORE_REALLOC;
  uint8_t factor = 1;
  size_t step = 0;

  LIMEADE_RECV ret;

  ret.data = malloc(alloc_inc * factor);

  while (0)
  {
    for (int i = 0; i < LIMEADE_RECV_CHUNKS_BEFORE_REALLOC; i++)
    {
      read(this.recv_fd, ret.data + step, LIMEADE_RECV_CHUNK_SZ);
      if (HAS_MAGIC(ret.data + step) == 0)
      {
        goto breakout;
      }
      step += LIMEADE_RECV_CHUNK_SZ;
    }

    factor++;
    ret.data = realloc(ret.data, alloc_inc * factor);
  }

breakout:

  ret.type = ((int*)ret.data)[sizeof(LIMEADE_MAGIC) + 2];
  ret.len  = step;

  return ret;
}

#undef HAS_MAGIC

static int limeade_delimeter_strlen(const char *x)
{
  /* Alternative to strlen that can be used internally against delimeters */
  char *i = (char*)x;
  int j = 0;
  while (1)
  {
    if (*i == *LIMEADE_FIELD_DELIM || *i == *LIMEADE_ROW_DELIM)
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
    limeade_inserror(LIMEADE_ERROR_DECODING_NONFATAL);
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

  ret = strtod(x, &status);

  if (*status != '\0')
  {
    limeade_inserror(LIMEADE_ERROR_DECODING_NONFATAL);
    return (double)0.00;
  }
  return ret;
}

// hiding the lack-of-namespace cludge
#define STOL(x) limeade_strol_err(x);
#define STOD(x) limeade_strol_err(x);
#define ALTSTRLEN(x) limeade_delimeter_strlen(x);
#define INC()                                       \
  i += j;                                           \
  if (i > (char*)data.data + data.len)              \
  {                                                 \
    limeade_inserror(LIMEADE_ERROR_DECODING_FATAL); \
    goto fatal;                                     \
  }                                                 \
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

// note about these functions:
// if the function fails, `data.data` will not be freed

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
  ret.sessionid = (uint64_t)STOL(sessionid_tmp);
  free(sessionid_tmp);

  return ret;
}

LIMEADE_EVENT limeade_process_event(LIMEADE_CONTEXT this, LIMEADE_RECV data)
{
  char *i = data.data;
  int j;
  i += 13;

  void *ts_s_tmp, *ts_ms_tmp, *pid_tmp, *retval_tmp;

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

free(data.data);

end:
  free(ts_s_tmp);
  free(ts_ms_tmp);
  free(pid_tmp);
  free(retval_tmp);
  return ret;

fatal:
  free(ret.syscall);
  free(ret.arg1);
  free(ret.arg2);
  goto end;
}

LIMEADE_EVENTS limeade_process_events(LIMEADE_CONTEXT this, LIMEADE_RECV data)
{

  char *i = data.data;
  int j, k;
  i += 13;
  LIMEADE_EVENT *l;

  void *nr_events_tmp, *ts_s_tmp, *ts_ms_tmp, *pid_tmp, *retval_tmp;

  LIMEADE_EVENTS ret;

  j = ALTSTRLEN(i) + 1;
  nr_events_tmp = malloc(j);
  memcpy(nr_events_tmp, i, j);
  ret.nr_events = STOL(nr_events_tmp);
  free(nr_events_tmp);

  ret.events = (LIMEADE_EVENT**)malloc(sizeof(LIMEADE_EVENT) * ret.nr_events);

  for(k = 0; k < ret.nr_events; k++)
  {
    l = ret.events[k];

    // unintializaing these pointers so we can check which ones we have to free
    // when backing out from an error

    ts_s_tmp = NULL;
    ts_ms_tmp = NULL;
    pid_tmp = NULL;
    retval_tmp = NULL;

    INC();
    ts_s_tmp = malloc(j);
    memcpy(ts_s_tmp, i, j);
    INC();
    ts_ms_tmp = malloc(j);
    memcpy(ts_ms_tmp, i, j);
    INC();
    pid_tmp = malloc(j);
    memcpy(pid_tmp, i, j);
    INC();
    l->syscall = malloc(j);
    memcpy(l->syscall, i, j);
    INC();
    l->arg1 = malloc(j);
    memcpy(l->arg1, i, j);
    INC();
    l->arg2 = malloc(j);
    memcpy(l->arg2, i, j);
    INC();
    retval_tmp = malloc(j);
    memcpy(retval_tmp, i, j);

    l->ts_s   = STOL(ts_s_tmp);
    l->ts_ms  = STOL(ts_ms_tmp);
    l->pid    = STOL(pid_tmp);
    l->retval = STOL(retval_tmp);

    free(ts_s_tmp);
    free(ts_ms_tmp);
    free(pid_tmp);
    free(retval_tmp);

    continue;

    fatal:

    if(ts_s_tmp)   free(ts_s_tmp);
    if(ts_ms_tmp)  free(ts_ms_tmp);
    if(pid_tmp)    free(pid_tmp);
    if(retval_tmp) free(retval_tmp);

    // now iterating backwards to free previous events in sequence
    for(k = k; k >= 0; k--)
    {
      l = ret.events[k];
      if(l->syscall) free(l->syscall);
      if(l->arg1)    free(l->arg1);
      if(l->arg2)    free(l->arg2);
    }
    free(ret.events);
    return ret;
  }
  free(data.data);
  return ret;
}

LIMEADE_PROC_GENERIC limeade_process_proc_generic(LIMEADE_CONTEXT this,
                                                  LIMEADE_RECV data)
{

  char *i = data.data;
  int j, k;
  i += 13;
  LIMEADE_PROC *l;

  void *ts_s_tmp, *ts_ms_tmp, *total_tmp;
  void *pid_tmp, *ppid_tmp, *uid_tmp, *threads_tmp, *cpu_ticks_tmp, *vm_rss_kb_tmp;

  LIMEADE_PROC_GENERIC ret;

  j = ALTSTRLEN(i) + 1;
  ts_s_tmp = malloc(j);
  memcpy(ts_s_tmp, i, j);
  INC();
  ts_ms_tmp = malloc(j);
  memcpy(ts_ms_tmp, i, j);
  INC();
  total_tmp = malloc(j);
  memcpy(total_tmp, i, j);
  ret.ts_s = STOL(ts_s_tmp);
  ret.ts_ms = STOL(ts_ms_tmp);
  ret.total = STOL(total_tmp);
  free(ts_s_tmp);
  free(ts_ms_tmp);
  free(total_tmp);

  ret.procs = (LIMEADE_PROC**)malloc(sizeof(LIMEADE_PROC) * ret.total);

  for(k = 0; k < ret.total; k++)
  {
    l = ret.procs[k];

    pid_tmp = NULL;
    ppid_tmp = NULL;
    uid_tmp = NULL;
    threads_tmp = NULL;
    cpu_ticks_tmp = NULL;
    vm_rss_kb_tmp = NULL;

    INC();
    pid_tmp = malloc(j);
    memcpy(pid_tmp, i, j);
    INC();
    ppid_tmp = malloc(j);
    memcpy(ppid_tmp, i, j);
    INC();
    uid_tmp = malloc(j);
    memcpy(uid_tmp, i, j);
    INC();
    threads_tmp = malloc(j);
    memcpy(threads_tmp, i, j);
    INC();
    vm_rss_kb_tmp = malloc(j);
    memcpy(vm_rss_kb_tmp, i, j);
    INC();
    l->command = malloc(j);
    memcpy(l->command, i, j);

    l->pid       = STOL(pid_tmp);
    l->ppid      = STOL(ppid_tmp);
    l->uid       = STOL(uid_tmp);
    l->threads   = STOL(threads_tmp);
    l->cpu_ticks = STOL(cpu_ticks_tmp);
    l->vm_rss_kb = STOL(vm_rss_kb_tmp);

    free(pid_tmp);
    free(ppid_tmp);
    free(uid_tmp);
    free(threads_tmp);
    free(cpu_ticks_tmp);
    free(vm_rss_kb_tmp);

    continue;

    fatal:

    if(pid_tmp) free(pid_tmp);
    if(ppid_tmp) free(ppid_tmp);
    if(uid_tmp)  free(uid_tmp);
    if(threads_tmp) free(threads_tmp);
    if(cpu_ticks_tmp) free(cpu_ticks_tmp);
    if(vm_rss_kb_tmp) free(vm_rss_kb_tmp);

    for(k = k; k >= 0; k++)
    {
      free(ret.procs[k]->command);
    }
    free(ret.procs);
    return ret;
  }

  free(data.data);
  return ret;
}

LIMEADE_PROC_UPDATE limeade_process_proc_update(LIMEADE_CONTEXT this,
                                                LIMEADE_RECV data)
{
  // TODO
}

LIMEADE_PERF limeade_process_perf(LIMEADE_CONTEXT this, LIMEADE_RECV data)
{
  char *i = data.data;
  int j;
  i += 13;

  void *ts_s_tmp, *ts_ms_tmp, *cores_tmp, *avg_cpu_pct_tmp, *mem_total_kb_tmp, *mem_free_kb_tmp;
  void *mem_available_kb_tmp, *mem_cached_kb_tmp, *load_1m_tmp, *load_5m_tmp, *load_15m_tmp;

  LIMEADE_PERF ret;

  j = ALTSTRLEN(i) + 1;
  ts_s_tmp = malloc(j);
  memcpy(ts_s_tmp, i, j);
  INC();
  ts_ms_tmp = malloc(j);
  memcpy(ts_ms_tmp, i, j);
  INC();
  cores_tmp = malloc(j);
  memcpy(cores_tmp, i, j);
  INC();
  avg_cpu_pct_tmp = malloc(j);
  memcpy(avg_cpu_pct_tmp, i, j);
  INC();
  mem_total_kb_tmp = malloc(j);
  memcpy(mem_total_kb_tmp, i, j);
  INC();
  mem_free_kb_tmp = malloc(j);
  memcpy(mem_free_kb_tmp, i, j);
  INC();
  mem_available_kb_tmp = malloc(j);
  memcpy(mem_available_kb_tmp, i, j);
  INC();
  mem_cached_kb_tmp = malloc(j);
  memcpy(mem_cached_kb_tmp, i, j);
  INC();
  load_1m_tmp = malloc(j);
  memcpy(load_1m_tmp, i, j);
  INC();
  load_5m_tmp = malloc(j);
  memcpy(load_5m_tmp, i, j);
  INC();
  load_15m_tmp = malloc(j);
  memcpy(load_15m_tmp, i, j);
  INC();
  ret.cores_json = malloc(j);
  memcpy(ret.cores_json, i, j);

  ret.ts_s             = STOL(ts_s_tmp);
  ret.ts_ms            = STOL(ts_ms_tmp);
  ret.cores            = STOL(cores_tmp);
  ret.avg_cpu_pct      = STOL(avg_cpu_pct_tmp);
  ret.mem_total_kb     = STOL(mem_total_kb_tmp);
  ret.mem_free_kb      = STOL(mem_free_kb_tmp);
  ret.mem_available_kb = STOL(mem_available_kb_tmp);
  ret.mem_cached_kb    = STOL(mem_cached_kb_tmp);
  ret.load_1m          = STOD(load_1m_tmp);
  ret.load_5m          = STOD(load_5m_tmp);
  ret.load_15m         = STOD(load_15m_tmp);

  free(data.data);

  end:

  free(ts_s_tmp);
  free(ts_ms_tmp);
  free(cores_tmp);
  free(avg_cpu_pct_tmp);
  free(mem_total_kb_tmp);
  free(mem_free_kb_tmp);
  free(mem_available_kb_tmp);
  free(mem_cached_kb_tmp);
  free(load_1m_tmp);
  free(load_5m_tmp);
  free(load_15m_tmp);

  return ret;

  fatal:
  free(ret.cores_json);
  goto end;
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
  va_start(args, type);

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
        // TODO
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
