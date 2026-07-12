// encode.c
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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zlib.h>

#include <liblimeade.h>

/*** HELPERS ***/

// convenient way to move data around
typedef struct
{
  size_t len;
  void *data;
} LIMEADE_DATA;

// writer call error guard
#define LIMEADE_WERR(expr, ret)                                                \
  do                                                                           \
  {                                                                            \
    if ((expr) < 0)                                                            \
    {                                                                          \
      return limeade_fail((ret));                                              \
    }                                                                          \
  } while (0)

// shorthand for generic error handling
#define LIMEADE_CHECKERR(expr, ret)                                            \
  do                                                                           \
  {                                                                            \
    if (expr)                                                                  \
    {                                                                          \
      return (ret);                                                            \
    }                                                                          \
  } while (0)

// calculate characters required to render unsigned 64-bit
static size_t limeade_du64(unsigned long long value)
{
  size_t digits = 1;

  while (value >= 10)
  {
    value /= 10;
    digits++;
  }

  return digits;
}

// calculate characters required to render signed 64-bit
static size_t limeade_di64(long long value)
{
  unsigned long long magnitude;

  if (value >= 0)
  {
    return limeade_du64((unsigned long long)value);
  }

  magnitude = (unsigned long long)(-(value + 1)) + 1;
  return limeade_du64(magnitude) + 1;
}

// calculate bytes to render string (strlen wrapper)
static size_t limeade_szs(const char *value)
{
  return ((value != NULL) ? strlen(value) : 0) + 1;
}

// calculate bytes to render signed number
static size_t limeade_szi(long long value) { return limeade_di64(value) + 1; }

// calculate bytes to render unsigned number
static size_t limeade_szu(unsigned long long value)
{
  return limeade_du64(value) + 1;
}

// calculate bytes to render double
static int limeade_szd(double value, size_t *size_out)
{
  int n = snprintf(NULL, 0, "%.17g", value);

  if (n < 0)
  {
    return -1;
  }

  *size_out = (size_t)n + 1;
  return 0;
}

// shorthand for string write
static void limeade_wrs(char *dst, const char *value, char delim)
{
  const char *scan = (value != NULL) ? value : "";

  while (*scan != '\0')
  {
    char ch = *scan++;

    if (ch == LIMEADE_FIELD_DELIM || ch == LIMEADE_ROW_DELIM)
    {
      ch = ' ';
    }

    *dst++ = ch;
  }

  *dst++ = delim;
}

// shorthand for signed integer write
static int limeade_wri(char *dst, size_t size, long long value, char delim)
{
  if (snprintf(dst, size, "%lld", value) < 0)
  {
    return -1;
  }

  dst[size - 1] = delim;
  return 0;
}

// shorthand for unsigned integer write
static int limeade_wru(char *dst, size_t size, unsigned long long value,
                       char delim)
{
  if (snprintf(dst, size, "%llu", value) < 0)
  {
    return -1;
  }

  dst[size - 1] = delim;
  return 0;
}

// shortahnd for double write
static int limeade_wrd(char *dst, size_t size, double value, char delim)
{
  if (snprintf(dst, size, "%.17g", value) < 0)
  {
    return -1;
  }

  dst[size - 1] = delim;
  return 0;
}

// shorthand for failure
inline static LIMEADE_DATA limeade_fail(LIMEADE_DATA ret)
{
  free(ret.data);
  ret.len = 0;
  ret.data = NULL;
  return ret;
}

LIMEADE_PACKET_FLAGS limeade_genflags(LIMEADE_PACKET_TYPE type,
                                      unsigned int datasz_before_compression,
                                      unsigned int datasz_after_compression)
{
  /* Generates flag datatype from given packet information */
  return (LIMEADE_PACKET_FLAGS){
      .version = LIMEADE_CHECKERROTOCOL_VERSION,
      .type = type,
      .datasz_before_compression =
          (datasz_before_compression < 0b11111111111111)
              ? datasz_before_compression
              : 0,
      .datasz_after_compression = (datasz_after_compression < 0b11111111111111)
                                      ? datasz_after_compression
                                      : 0,
      .field_delim = LIMEADE_FIELD_DELIM,
      .row_delim = LIMEADE_ROW_DELIM};
}

/*** BIG HELPERS ***/

LIMEADE_DATA limeade_gendata_sysoverv(LIMEADE_PACKET_SYSOVERV in)
{
  /* Converts LIMEADE_PACKET_SYSOVERV data to an buffer that can be
   * compressed to be used as the packet's data segment.
   * Returns a LIMEADE_DATA object for simplicity.
   */

  LIMEADE_DATA ret = {0, NULL};
  size_t hostnamesz, kernelversz, distrosz, ipaddrsz, macaddrsz, processorsz;
  size_t processor_vendsz, ram_gbssz;
  char *data_index;

  // calculate sizes
  hostnamesz = limeade_szs(in.hostname);
  kernelversz = limeade_szs(in.kernelver);
  distrosz = limeade_szs(in.distro);
  ipaddrsz = limeade_szs(in.ipaddr);
  macaddrsz = limeade_szs(in.macaddr);
  processorsz = limeade_szs(in.processor);
  processor_vendsz = limeade_szs(in.processor_vend);
  ram_gbssz = limeade_szu((unsigned long long)in.ram_gbs);

  ret.len = hostnamesz + kernelversz + distrosz + ipaddrsz + macaddrsz;
  ret.len += processorsz + processor_vendsz + ram_gbssz;

  ret.data = malloc(ret.len);
  if (ret.data == NULL)
  {
    return ret;
  }

  data_index = (char *)ret.data;

  // write + increment
  limeade_wrs(data_index, in.hostname, LIMEADE_FIELD_DELIM);
  data_index += hostnamesz;
  limeade_wrs(data_index, in.kernelver, LIMEADE_FIELD_DELIM);
  data_index += kernelversz;
  limeade_wrs(data_index, in.distro, LIMEADE_FIELD_DELIM);
  data_index += distrosz;
  limeade_wrs(data_index, in.ipaddr, LIMEADE_FIELD_DELIM);
  data_index += ipaddrsz;
  limeade_wrs(data_index, in.macaddr, LIMEADE_FIELD_DELIM);
  data_index += macaddrsz;
  limeade_wrs(data_index, in.processor, LIMEADE_FIELD_DELIM);
  data_index += processorsz;
  limeade_wrs(data_index, in.processor_vend, LIMEADE_FIELD_DELIM);
  data_index += processor_vendsz;
  LIMEADE_WERR(limeade_wru(data_index, ram_gbssz,
                           (unsigned long long)in.ram_gbs, LIMEADE_ROW_DELIM),
               ret);
  data_index += ram_gbssz;

  return ret;
}

LIMEADE_DATA limeade_gendata_events(LIMEADE_PACKET_EVENTS in)
{
  /* Converts LIMEADE_PACKET_EVENTS data into a buffer that can be
   * compressed to be used as packet's data segment.
   * Returns as LIMEADE_DATA for simplicity.
   */

  // sanity rq
  if (in.events == NULL)
  {
    return ret;
  }

  // defs
  LIMEADE_DATA ret = {0, NULL};
  char *data_index;
  size_t ts_ssz, ts_mssz, pidsz, typesz, subtypesz, arg1sz, arg2sz, retvalsz, i;

  in.nr_events;

  for (i = 0; i < in.nr_events; i++)
  {
    EVENT *ev;

    if (in.events[i] == NULL || *in.events[i] == NULL) // bad data
    {
      return ret;
    }

    ev = *in.events[i];
    ts_ssz = limeade_szu((unsigned long long)ev->ts.s);
    ts_mssz = limeade_szu((unsigned long long)ev->ts.ms);
    pidsz = limeade_szi((long long)ev->pid);
    typesz = limeade_szs(ev->type);
    subtypesz = limeade_szs(ev->subtype);
    arg1sz = limeade_szs(ev->arg1);
    arg2sz = limeade_szs(ev->arg2);

    retvalsz = limeade_szi((long long)ev->retval);

    ret.len += ts_ssz + ts_mssz + pidsz + typesz + subtypesz + arg1sz + arg2sz +
               retvalsz;
  }

  if (ret.len == 0)
  {
    return ret;
  }

  ret.data = malloc(ret.len);
  if (ret.data == NULL)
  {
    ret.len = 0;
    return ret;
  }

  data_index = (char *)ret.data;

  for (i = 0; i < in.nr_events; i++)
  {
    EVENT *ev = *in.events[i];

    ts_ssz = limeade_szu((unsigned long long)ev->ts.s);
    ts_mssz = limeade_szu((unsigned long long)ev->ts.ms);
    pidsz = limeade_szi((long long)ev->pid);
    typesz = limeade_szs(ev->type);
    subtypesz = limeade_szs(ev->subtype);
    arg1sz = limeade_szs(ev->arg1);
    arg2sz = limeade_szs(ev->arg2);
    retvalsz = limeade_szi((long long)ev->retval);

    LIMEADE_WERR(limeade_wru(data_index, ts_ssz, (unsigned long long)ev->ts.s,
                             LIMEADE_FIELD_DELIM),
                 ret);
    data_index += ts_ssz;
    LIMEADE_WERR(limeade_wru(data_index, ts_mssz, (unsigned long long)ev->ts.ms,
                             LIMEADE_FIELD_DELIM),
                 ret);
    data_index += ts_mssz;
    LIMEADE_WERR(
        limeade_wri(data_index, pidsz, (long long)ev->pid, LIMEADE_FIELD_DELIM),
        ret);
    data_index += pidsz;

    limeade_wrs(data_index, ev->type, LIMEADE_FIELD_DELIM);
    data_index += typesz;
    limeade_wrs(data_index, ev->subtype, LIMEADE_FIELD_DELIM);
    data_index += subtypesz;
    limeade_wrs(data_index, ev->arg1, LIMEADE_FIELD_DELIM);
    data_index += arg1sz;
    limeade_wrs(data_index, ev->arg2, LIMEADE_FIELD_DELIM);
    data_index += arg2sz;
    LIMEADE_WERR(limeade_wri(data_index, retvalsz, (long long)ev->retval,
                             LIMEADE_ROW_DELIM),
                 ret);
    data_index += retvalsz;
  }

  return ret;
}

LIMEADE_DATA limeade_gendata_procs_generic(LIMEADE_PACKET_PROCS_GENERIC in)
{
  LIMEADE_DATA ret = {0, NULL};
  char *data_index;
  size_t ts_ssz, ts_mssz, pidsz, ppidsz, uidsz, threadssz, cpu_tickssz,
      vm_rss_kbsz, commsz, i;
  size_t nr_processes;

  if (in.processes == NULL)
  {
    return ret;
  }

  ts_ssz = limeade_szu((unsigned long long)in.ts.s);
  ts_mssz = limeade_szu((unsigned long long)in.ts.ms);

  nr_processes = in.nr_processes;

  for (i = 0; i < nr_processes; i++)
  {
    LIMEADE_PACKET_PROC *proc;

    if (in.processes[i] == NULL)
    {
      return ret;
    }

    proc = (LIMEADE_PACKET_PROC *)in.processes[i];

    pidsz = limeade_szi((long long)proc->pid);
    ppidsz = limeade_szi((long long)proc->ppid);
    uidsz = limeade_szu((unsigned long long)proc->uid);
    threadssz = limeade_szu((unsigned long long)proc->threads);
    cpu_tickssz = limeade_szu((unsigned long long)proc->cpu_ticks);
    vm_rss_kbsz = limeade_szu((unsigned long long)proc->vm_rss_kb);
    commsz = limeade_szs(proc->comm);

    ret.len += ts_ssz + ts_mssz + pidsz + ppidsz + uidsz + threadssz +
               cpu_tickssz + vm_rss_kbsz + commsz;
  }

  if (ret.len == 0)
  {
    return ret;
  }

  ret.data = malloc(ret.len);
  if (ret.data == NULL)
  {
    ret.len = 0;
    return ret;
  }

  data_index = (char *)ret.data;

  for (i = 0; i < nr_processes; i++)
  {
    LIMEADE_PACKET_PROC *proc = (LIMEADE_PACKET_PROC *)in.processes[i];

    pidsz = limeade_szi((long long)proc->pid);
    ppidsz = limeade_szi((long long)proc->ppid);
    uidsz = limeade_szu((unsigned long long)proc->uid);
    threadssz = limeade_szu((unsigned long long)proc->threads);
    cpu_tickssz = limeade_szu((unsigned long long)proc->cpu_ticks);
    vm_rss_kbsz = limeade_szu((unsigned long long)proc->vm_rss_kb);
    commsz = limeade_szs(proc->comm);

    LIMEADE_WERR(limeade_wru(data_index, ts_ssz, (unsigned long long)in.ts.s,
                             LIMEADE_FIELD_DELIM),
                 ret);
    data_index += ts_ssz;
    LIMEADE_WERR(limeade_wru(data_index, ts_mssz, (unsigned long long)in.ts.ms,
                             LIMEADE_FIELD_DELIM),
                 ret);
    data_index += ts_mssz;
    LIMEADE_WERR(limeade_wri(data_index, pidsz, (long long)proc->pid,
                             LIMEADE_FIELD_DELIM),
                 ret);
    data_index += pidsz;
    LIMEADE_WERR(limeade_wri(data_index, ppidsz, (long long)proc->ppid,
                             LIMEADE_FIELD_DELIM),
                 ret);
    data_index += ppidsz;
    LIMEADE_WERR(limeade_wru(data_index, uidsz, (unsigned long long)proc->uid,
                             LIMEADE_FIELD_DELIM),
                 ret);
    data_index += uidsz;
    LIMEADE_WERR(limeade_wru(data_index, threadssz,
                             (unsigned long long)proc->threads,
                             LIMEADE_FIELD_DELIM),
                 ret);
    data_index += threadssz;
    LIMEADE_WERR(limeade_wru(data_index, cpu_tickssz,
                             (unsigned long long)proc->cpu_ticks,
                             LIMEADE_FIELD_DELIM),
                 ret);
    data_index += cpu_tickssz;
    LIMEADE_WERR(limeade_wru(data_index, vm_rss_kbsz,
                             (unsigned long long)proc->vm_rss_kb,
                             LIMEADE_FIELD_DELIM),
                 ret);
    data_index += vm_rss_kbsz;

    limeade_wrs(data_index, proc->comm, LIMEADE_ROW_DELIM);
    data_index += commsz;
  }

  return ret;
}

LIMEADE_DATA
limeade_gendata_procs_update(LIMEADE_PACKET_PROCS_UPDATE in) // NOT IMPLEMENTED
{
  // TODO
}

LIMEADE_DATA limeade_gendata_perf(LIMEADE_PACKET_PERF in)
{
  LIMEADE_DATA ret = {0, NULL};
  char *data_index;
  size_t ts_ssz, ts_mssz, coressz, avg_cpu_pctsz, mem_total_kbsz, mem_free_kbsz;
  size_t mem_available_kbsz, mem_cached_kbsz, load_1msz, load_5msz, load_15msz,
      cores_jsonsz;

  ts_ssz = limeade_szu((unsigned long long)in.ts.s);
  ts_mssz = limeade_szu((unsigned long long)in.ts.ms);
  coressz = limeade_szu((unsigned long long)in.cores);
  avg_cpu_pctsz = limeade_szu((unsigned long long)in.avg_cpu_pct);
  mem_total_kbsz = limeade_szu((unsigned long long)in.mem_total_kb);
  mem_free_kbsz = limeade_szu((unsigned long long)in.mem_free_kb);
  mem_available_kbsz = limeade_szu((unsigned long long)in.mem_available_kb);
  mem_cached_kbsz = limeade_szu((unsigned long long)in.mem_cached_kb);
  cores_jsonsz = limeade_szs(in.cores_json);

  if (limeade_szd(in.load_1m, &load_1msz) < 0 ||
      limeade_szd(in.load_5m, &load_5msz) < 0 ||
      limeade_szd(in.load_15m, &load_15msz) < 0)
  {
    return ret;
  }

  ret.len = ts_ssz + ts_mssz + coressz + avg_cpu_pctsz + mem_total_kbsz +
            mem_free_kbsz;
  ret.len += mem_available_kbsz + mem_cached_kbsz + load_1msz + load_5msz +
             load_15msz + cores_jsonsz;

  ret.data = malloc(ret.len);
  if (ret.data == NULL)
  {
    ret.len = 0;
    return ret;
  }

  data_index = (char *)ret.data;

  LIMEADE_WERR(limeade_wru(data_index, ts_ssz, (unsigned long long)in.ts.s,
                           LIMEADE_FIELD_DELIM),
               ret);
  data_index += ts_ssz;
  LIMEADE_WERR(limeade_wru(data_index, ts_mssz, (unsigned long long)in.ts.ms,
                           LIMEADE_FIELD_DELIM),
               ret);
  data_index += ts_mssz;
  LIMEADE_WERR(limeade_wru(data_index, coressz, (unsigned long long)in.cores,
                           LIMEADE_FIELD_DELIM),
               ret);
  data_index += coressz;
  LIMEADE_WERR(limeade_wru(data_index, avg_cpu_pctsz,
                           (unsigned long long)in.avg_cpu_pct,
                           LIMEADE_FIELD_DELIM),
               ret);
  data_index += avg_cpu_pctsz;
  LIMEADE_WERR(limeade_wru(data_index, mem_total_kbsz,
                           (unsigned long long)in.mem_total_kb,
                           LIMEADE_FIELD_DELIM),
               ret);
  data_index += mem_total_kbsz;
  LIMEADE_WERR(limeade_wru(data_index, mem_free_kbsz,
                           (unsigned long long)in.mem_free_kb,
                           LIMEADE_FIELD_DELIM),
               ret);
  data_index += mem_free_kbsz;
  LIMEADE_WERR(limeade_wru(data_index, mem_available_kbsz,
                           (unsigned long long)in.mem_available_kb,
                           LIMEADE_FIELD_DELIM),
               ret);
  data_index += mem_available_kbsz;
  LIMEADE_WERR(limeade_wru(data_index, mem_cached_kbsz,
                           (unsigned long long)in.mem_cached_kb,
                           LIMEADE_FIELD_DELIM),
               ret);
  data_index += mem_cached_kbsz;
  LIMEADE_WERR(
      limeade_wrd(data_index, load_1msz, in.load_1m, LIMEADE_FIELD_DELIM), ret);
  data_index += load_1msz;
  LIMEADE_WERR(
      limeade_wrd(data_index, load_5msz, in.load_5m, LIMEADE_FIELD_DELIM), ret);
  data_index += load_5msz;
  LIMEADE_WERR(
      limeade_wrd(data_index, load_15msz, in.load_15m, LIMEADE_FIELD_DELIM),
      ret);
  data_index += load_15msz;

  limeade_wrs(data_index, in.cores_json, LIMEADE_ROW_DELIM);
  data_index += cores_jsonsz;

  return ret;
}

LIMEADE_DATA limeade_gendata_ask(LIMEADE_PACKET_ASK in)
{
  LIMEADE_DATA ret = {0, NULL};
  char *data_index;
  size_t is_new_devicesz, is_new_sessionsz, ts_ssz, ts_mssz;

  is_new_devicesz = limeade_szu((unsigned long long)in.is_new_device);
  is_new_sessionsz = limeade_szu((unsigned long long)in.is_new_session);
  ts_ssz = limeade_szu((unsigned long long)in.ts.s);
  ts_mssz = limeade_szu((unsigned long long)in.ts.ms);

  ret.len = is_new_devicesz + is_new_sessionsz + ts_ssz + ts_mssz;

  ret.data = malloc(ret.len);
  if (ret.data == NULL)
  {
    return ret;
  }

  data_index = (char *)ret.data;
  LIMEADE_WERR(limeade_wru(data_index, is_new_devicesz,
                           (unsigned long long)in.is_new_device,
                           LIMEADE_FIELD_DELIM),
               ret);
  data_index += is_new_devicesz;
  LIMEADE_WERR(limeade_wru(data_index, is_new_sessionsz,
                           (unsigned long long)in.is_new_session,
                           LIMEADE_FIELD_DELIM),
               ret);
  data_index += is_new_sessionsz;
  LIMEADE_WERR(limeade_wru(data_index, ts_ssz, (unsigned long long)in.ts.s,
                           LIMEADE_FIELD_DELIM),
               ret);
  data_index += ts_ssz;
  LIMEADE_WERR(limeade_wru(data_index, ts_mssz, (unsigned long long)in.ts.ms,
                           LIMEADE_ROW_DELIM),
               ret);
  data_index += ts_mssz;

  return ret;
}

LIMEADE_DATA limeade_gendata_answer(LIMEADE_PACKET_ANSWER in)
{
  LIMEADE_DATA ret = {0, NULL};
  char *data_index;
  size_t acceptedsz = limeade_szu((unsigned long long)(in.accepted ? 1 : 0));

  ret.len = acceptedsz;
  ret.data = malloc(ret.len);
  if (ret.data == NULL)
  {
    return ret;
  }

  data_index = (char *)ret.data;
  LIMEADE_WERR(limeade_wru(data_index, acceptedsz,
                           (unsigned long long)(in.accepted ? 1 : 0),
                           LIMEADE_ROW_DELIM),
               ret);
  data_index += acceptedsz;

  return ret;
}

LIMEADE_DATA limeade_gendata_commandeer(LIMEADE_PACKET_COMMANDEER in)
{
  LIMEADE_DATA ret = {0, NULL};
  char *data_index;
  size_t commandsz, require_ttysz, require_adminsz;

  commandsz = limeade_szs(in.command);
  require_ttysz = limeade_szu((unsigned long long)in.require_tty);
  require_adminsz = limeade_szu((unsigned long long)in.require_admin);

  ret.len = commandsz + require_ttysz + require_adminsz;

  ret.data = malloc(ret.len);
  if (ret.data == NULL)
  {
    return ret;
  }

  data_index = (char *)ret.data;
  limeade_wrs(data_index, in.command, LIMEADE_FIELD_DELIM);
  data_index += commandsz;
  LIMEADE_WERR(limeade_wru(data_index, require_ttysz,
                           (unsigned long long)in.require_tty,
                           LIMEADE_FIELD_DELIM),
               ret);
  data_index += require_ttysz;
  LIMEADE_WERR(limeade_wru(data_index, require_adminsz,
                           (unsigned long long)in.require_admin,
                           LIMEADE_ROW_DELIM),
               ret);
  data_index += require_adminsz;

  return ret;
}

LIMEADE_PACKET limeade_compile(char *magic, LIMEADE_PACKET_FLAGS flags,
                               LIMEADE_SESSION session_id, void *data)
{
  /* Turns packet segments into a full packet */
  LIMEADE_PACKET ret = {0, NULL};
  const unsigned char *magic_src;
  const size_t magic_len = sizeof(LIMEADE_MAGIC);
  const size_t flags_len = sizeof(LIMEADE_PACKET_FLAGS);
  const size_t session_len = (flags.type == LIMEADE_CLIENT_ASK) ? 0 : 5;
  size_t data_len;
  size_t total_len;
  unsigned char *dst;

  data_len = (flags.datasz_after_compression > 0)
                 ? (size_t)flags.datasz_after_compression
                 : (size_t)flags.datasz_before_compression;

  if (magic != NULL)
  {
    magic_src = (const unsigned char *)magic;
  }
  else
  {
    magic_src = (const unsigned char *)LIMEADE_MAGIC;
  }

  LIMEADE_CHECKERR(session_len > 0 && session_id == NULL, ret);
  LIMEADE_CHECKERR(data_len > 0 && data == NULL, ret);
  LIMEADE_CHECKERR(SIZE_MAX - magic_len < flags_len, ret);

  total_len = magic_len + flags_len;

  LIMEADE_CHECKERR(SIZE_MAX - total_len < session_len, ret);

  total_len += session_len;

  LIMEADE_CHECKERR(SIZE_MAX - total_len < data_len, ret);

  total_len += data_len;

  LIMEADE_CHECKERR(total_len > UINT32_MAX, ret);

  ret.data = (unsigned char *)malloc(total_len);
  LIMEADE_CHECKERR(ret.data == NULL, ret);

  ret.sz = (uint32_t)total_len;
  dst = ret.data;

  memcpy(dst, magic_src, magic_len);
  dst += magic_len;

  memcpy(dst, &flags, flags_len);
  dst += flags_len;

  if (session_len > 0)
  {
    memcpy(dst, session_id, session_len);
    dst += session_len;
  }

  if (data_len > 0)
  {
    memcpy(dst, data, data_len);
  }

  return ret;
}

LIMEADE_DATA limeade_compress(LIMEADE_DATA *in)
{
  /* Wrapper for DEFLATE @ compression level 5, used for packet data */
  LIMEADE_DATA ret = {0, NULL};
  size_t cap;
  uLong src_len;
  uLongf dst_len;
  int zret;

  LIMEADE_CHECKERR(in == NULL, ret);
  LIMEADE_CHECKERR(in->data == NULL || in->len == 0, ret);
  LIMEADE_CHECKERR(in->len > (size_t)ULONG_MAX, ret);

  src_len = (uLong)in->len;
  cap = (size_t)compressBound(src_len);
  LIMEADE_CHECKERR(cap == 0, ret);

  ret.data = malloc(cap);
  LIMEADE_CHECKERR(ret.data == NULL, ret);

  dst_len = (uLongf)cap;
  zret = compress2((Bytef *)ret.data, &dst_len, (const Bytef *)in->data,
                   src_len, 5);
  if (zret != Z_OK)
  {
    return limeade_fail(ret);
  }

  ret.len = (size_t)dst_len;
  return ret;
}

/*** DEFINITIONS ***/

LIMEADE_PACKET limeade_compile_sysoverv(LIMEADE_PACKET_SYSOVERV in)
{
  LIMEADE_PACKET ret = {0, NULL};
  LIMEADE_DATA data_raw = {0, NULL};
  LIMEADE_DATA data_cmp = {0, NULL};
  LIMEADE_PACKET_FLAGS flags;
  LIMEADE_SESSION session_id = {0, 0, 0, 0, 0};
  const void *payload;
  size_t payload_len;
  unsigned int before_len;
  unsigned int after_len;

  data_raw = limeade_gendata_sysoverv(in);
  LIMEADE_CHECKERR(data_raw.data == NULL || data_raw.len == 0, ret);
  LIMEADE_CHECKERR(data_raw.len > (size_t)UINT_MAX, ret);

  data_cmp = limeade_compress(&data_raw);

  if (data_cmp.data != NULL && data_cmp.len > 0 && data_cmp.len < data_raw.len)
  {
    payload = data_cmp.data;
    payload_len = data_cmp.len;
    after_len = (unsigned int)data_cmp.len;
  }
  else
  {
    payload = data_raw.data;
    payload_len = data_raw.len;
    after_len = 0;
  }

  LIMEADE_CHECKERR(payload_len > (size_t)UINT_MAX, ret);

  before_len = (unsigned int)data_raw.len;
  flags = limeade_genflags(LIMEADE_CLIENT_SYSOVERV, before_len, after_len);
  ret = limeade_compile(NULL, flags, session_id, (void *)payload);

  free(data_cmp.data);
  free(data_raw.data);

  return ret;
}

LIMEADE_PACKET limeade_compile_events(LIMEADE_PACKET_EVENTS in)
{
  // TODO
}

LIMEADE_PACKET limeade_compile_procs_generic(LIMEADE_PACKET_PROCS_GENERIC in)
{
  // TODO
}

LIMEADE_PACKET
limeade_compile_procs_update(LIMEADE_PACKET_PROCS_UPDATE in) // NOT IMPLEMENTED
{
  // TODO
}

LIMEADE_PACKET limeade_compile_perf(LIMEADE_PACKERF_PERF in)
{
  // TODO
}

LIMEADE_PACKET limeade_compile_ask(LIMEADE_PACKET_ASK in)
{
  // TODO
}

LIMEADE_PACKET limeade_compile_answer(LIMEADE_PACKET_ANSWER in)
{
  // TODO
}

LIMEADE_PACKET limeade_compile_commandeer(LIMEADE_PACKET_COMMANDEER in)
{
  // TODO
}
