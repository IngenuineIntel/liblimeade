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

  if (value >= 0) // just treat as unsigned
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
  // TODO workaround for snprintf?
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

    // IMPORTANT: escaping delimeters
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

/*** BIG HELPERS ***/

LIMEADE_PACKET_FLAGS limeade_genflags(LIMEADE_PACKET_TYPE type,
                                      unsigned int datasz_before_compression,
                                      unsigned int datasz_after_compression)
{
  /* Generates flag datatype from given packet information */
  // TODO write in a way that doesn't make clang-format get hysterical
  return (LIMEADE_PACKET_FLAGS){
      .version     = POTOCOL_VERSION,
      .type        = type,
      .datasz_before_compression = datasz_before_compression
      .datasz_after_compression  = datasz_after_compression
      .field_delim = LIMEADE_FIELD_DELIM,
      .row_delim   = LIMEADE_ROW_DELIM};
}

// this macro is used in the limeade_gendata_* functions
#define CHECK(expr, err)                                                       \
  if((expr) != 0)                                                               \
  {                                                                            \
    free(ret.data);                                                            \
    ret.len = 0;                                                               \
    ret.data = NULL;                                                           \
    limeade_inserror(err);                                                     \
    return ret;                                                                \
  }

LIMEADE_DATA limeade_gendata_sysoverv(LIMEADE_PACKET_SYSOVERV in)
{
  /* Converts LIMEADE_PACKET_SYSOVERV data to an buffer that can be
   * compressed to be used as the packet's data segment.
   * Returns a LIMEADE_DATA object.
   */

  LIMEADE_DATA ret = {0, NULL};
  size_t hostnamesz, kernelversz, distrosz, ipaddrsz, macaddrsz, processorsz;
  size_t processor_vendsz, ram_gbssz;
  char *data_index;

  // calculate sizes
  hostnamesz       = limeade_szs(in.hostname);
  kernelversz      = limeade_szs(in.kernelver);
  distrosz         = limeade_szs(in.distro);
  ipaddrsz         = limeade_szs(in.ipaddr);
  macaddrsz        = limeade_szs(in.macaddr);
  processorsz      = limeade_szs(in.processor);
  processor_vendsz = limeade_szs(in.processor_vend);
  ram_gbssz        = limeade_szu((unsigned long long)in.ram_gbs);

  ret.len =  hostnamesz + kernelversz + distrosz         + ipaddrsz;
  ret.len += macaddrsz  + processorsz + processor_vendsz + ram_gbssz;

  ret.data = malloc(ret.len);

  // index
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

  CHECK(limeade_wru(data_inedx, ram_gbssz, (unsigned long long)in.ram_gbs, LIMEADE_ROW_DELIM),
        LIMEADE_ERROR_GARBAGE);
          
  return ret;
}

LIMEADE_DATA limeade_gendata_events(LIMEADE_PACKET_EVENTS in)
{
  /* Converts LIMEADE_PACKET_EVENTS data into a buffer that can be
   * compressed to be used as packet's data segment.
   * Returns as LIMEADE_DATA for simplicity.
   */
  
  LIMEADE_DATA ret = {0, NULL};
  char *data_index;
  size_t ts_ssz, ts_mssz, pidsz, typesz, subtypesz, arg1sz, arg2sz, retvalsz, i;

  CHECK(!in || !in.events, LIMEADE_ERROR_GARBAGE);

  // buffer size calculation
  for (i = 0; i < in.nr_events; i++)
  {
    EVENT *ev;

    ev = *in.events[i];

    ts_ssz    = limeade_szu((unsigned long long)ev->ts.s);
    ts_mssz   = limeade_szu((unsigned long long)ev->ts.ms);
    pidsz     = limeade_szi((long long)ev->pid);
    typesz    = limeade_szs(ev->type);
    subtypesz = limeade_szs(ev->subtype);
    arg1sz    = limeade_szs(ev->arg1);
    arg2sz    = limeade_szs(ev->arg2);
    retvalsz  = limeade_szi((long long)ev->retval);

    ret.len += ts_ssz    + ts_mssz + pidsz  + typesz;
    ret.len += subtypesz + arg1sz  + arg2sz + retvalsz;
  }

  ret.data = malloc(ret.len);

  data_index = (char *)ret.data;

  // buffer write loop
  for (i = 0; i < in.nr_events; i++)
  {
    EVENT *ev = *in.events[i];

    ts_ssz    = limeade_szu((unsigned long long)ev->ts.s);
    ts_mssz   = limeade_szu((unsigned long long)ev->ts.ms);
    pidsz     = limeade_szi((long long)ev->pid);
    typesz    = limeade_szs(ev->type);
    subtypesz = limeade_szs(ev->subtype);
    arg1sz    = limeade_szs(ev->arg1);
    arg2sz    = limeade_szs(ev->arg2);
    retvalsz  = limeade_szi((long long)ev->retval);

    CHECK(limeade_wru(data_index, ts_ssz, (unsigned long long)ev->ts.s, LIMEADE_FIELD_DELIM),
          LIMEADE_ERROR_GARBAGE);
    data_index += ts_ssz;
    
    CHECK(limeade_wru(data_index, ts_mssz, (unsigned long long)ev->ts.ms, LIMEADE_FIELD_DELIM),
          LIMEADE_ERROR_GARBAGE);
    data_index += ts_mssz;
    
    CHECK(limeade_wri(data_index, pidsz, (long long)ev->pid, LIMEADE_FIELD_DELIM),
          LIMEADE_ERROR_GARBAGE);
    data_index += pidsz;

    limeade_wrs(data_index, ev->type, LIMEADE_FIELD_DELIM);
    data_index += typesz;

    limeade_wrs(data_index, ev->subtype, LIMEADE_FIELD_DELIM);
    data_index += subtypesz;

    limeade_wrs(data_index, ev->arg1, LIMEADE_FIELD_DELIM);
    data_index += arg1sz;

    limeade_wrs(data_index, ev->arg2, LIMEADE_FIELD_DELIM);
    data_index += arg2sz;

    CHECK(limeade_wri(data_index, retvalsz, (long long)ev->retval, LIMEADE_ROW_DELIM),
          LIMEADE_ERROR_GARBAGE);
    data_index += retvalsz;
  
  }

  return ret;
}

LIMEADE_DATA limeade_gendata_procs_generic(LIMEADE_PACKET_PROCS_GENERIC in)
{
  LIMEADE_DATA ret = {0, NULL};
  char *data_index;
  size_t ts_ssz, ts_mssz, pidsz, ppidsz, uidsz, threadssz, cpu_tickssz;
  size_t vm_rss_kbsz, commsz, i;

  CHECK(!in || !in.processes, LIMEADE_ERROR_GARBAGE);

  ts_ssz  = limeade_szu((unsigned long long)in.ts.s);
  ts_mssz = limeade_szu((unsigned long long)in.ts.ms);

  for (i = 0; i < in.nr_processes; i++)
  {
    LIMEADE_PACKET_PROC *proc;

    proc = (LIMEADE_PACKET_PROC *)in.processes[i];

    pidsz       = limeade_szi((long long)proc->pid);
    ppidsz      = limeade_szi((long long)proc->ppid);
    uidsz       = limeade_szu((unsigned long long)proc->uid);
    threadssz   = limeade_szu((unsigned long long)proc->threads);
    cpu_tickssz = limeade_szu((unsigned long long)proc->cpu_ticks);
    vm_rss_kbsz = limeade_szu((unsigned long long)proc->vm_rss_kb);
    commsz      = limeade_szs(proc->comm);

    ret.len += ts_ssz + ts_mssz + pidsz + ppidsz + uidsz + threadssz +
               cpu_tickssz + vm_rss_kbsz + commsz;
  }

  ret.data = malloc(ret.len);

  data_index = (char *)ret.data;

  for (i = 0; i < nr_processes; i++)
  {
    LIMEADE_PACKET_PROC *proc = (LIMEADE_PACKET_PROC *)in.processes[i];

    pidsz       = limeade_szi((long long)proc->pid);
    ppidsz      = limeade_szi((long long)proc->ppid);
    uidsz       = limeade_szu((unsigned long long)proc->uid);
    threadssz   = limeade_szu((unsigned long long)proc->threads);
    cpu_tickssz = limeade_szu((unsigned long long)proc->cpu_ticks);
    vm_rss_kbsz = limeade_szu((unsigned long long)proc->vm_rss_kb);
    commsz      = limeade_szs(proc->comm);

    CHECK(limeade_wru(data_index, ts_ssz, (unsigned long long)in.ts.s, LIMEADE_FIELD_DELIM),
          LIMEADE_ERROR_GARBAGE);
    data_index += ts_ssz;

    CHECK(limeade_wru(data_index, ts_mssz, (unsigned long long)in.ts.ms, LIMEADE_FIELD_DELIM),
          LIMEADE_ERROR_GARBAGE);
    data_index += ts_mssz;
    
    CHECK(limeade_wri(data_index, pidsz, (long long)proc->pid,
                      LIMEADE_FIELD_DELIM),
          LIMEADE_ERROR_GARBAGE);
    data_index += pidsz;
    
    CHECK(limeade_wri(data_index, ppidsz, (long long)proc->ppid,
                      LIMEADE_FIELD_DELIM),
                 ret);
    data_index += ppidsz;
    
    CHECK(limeade_wru(data_index, uidsz, (unsigned long long)proc->uid,
                      LIMEADE_FIELD_DELIM),
          LIMEADE_ERROR_GARBAGE);
    data_index += uidsz;
    
    CHECK(limeade_wru(data_index, threadssz,
                      (unsigned long long)proc->threads, LIMEADE_FIELD_DELIM),
          LIMEADE_ERROR_GARBAGE);
    data_index += threadssz;
    
    CHECK(limeade_wru(data_index, cpu_tickssz,
                      (unsigned long long)proc->cpu_ticks, LIMEADE_FIELD_DELIM),
          LIMEADE_ERROR_GARBAGE);
    data_index += cpu_tickssz;
    
    CHECK(limeade_wru(data_index, vm_rss_kbsz,
                      (unsigned long long)proc->vm_rss_kb, LIMEADE_FIELD_DELIM),
          LIMEADE_ERROR_GARBAGE);
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

  ts_ssz             = limeade_szu((unsigned long long)in.ts.s);
  ts_mssz            = limeade_szu((unsigned long long)in.ts.ms);
  coressz            = limeade_szu((unsigned long long)in.cores);
  avg_cpu_pctsz      = limeade_szu((unsigned long long)in.avg_cpu_pct);
  mem_total_kbsz     = limeade_szu((unsigned long long)in.mem_total_kb);
  mem_free_kbsz      = limeade_szu((unsigned long long)in.mem_free_kb);
  mem_available_kbsz = limeade_szu((unsigned long long)in.mem_available_kb);
  mem_cached_kbsz    = limeade_szu((unsigned long long)in.mem_cached_kb);
  cores_jsonsz       = limeade_szs(in.cores_json);

  ret.len =  ts_ssz + ts_mssz + coresz + avg_cpu_pctsz + mem_total_kbsz;
  ret.len += mem_free_kbsz + mem_available_kbsz + mem_cached_kbsz + load_1msz;
  ret.len += load_5msz + load_15msz + core_jsonsz;

  ret.data   = malloc(ret.len);
  data_index = (char *)ret.data;

  CHECK(limeade_wru(data_index, ts_ssz, (unsigned long long)in.ts.s,
                    LIMEADE_FIELD_DELIM),
      LIMEADE_ERROR_GARBAGE);
  data_index += ts_ssz;

  CHECK(limeade_wru(data_index, ts_mssz, (unsigned long long)in.ts.ms,
                    LIMEADE_FIELD_DELIM),
      LIMEADE_ERROR_GARBAGE);
  data_index += ts_mssz;

  CHECK(limeade_wru(data_index, coressz, (unsigned long long)in.cores,
                    LIMEADE_FIELD_DELIM),
      LIMEADE_ERROR_GARBAGE);
  data_index += coressz;

  CHECK(limeade_wru(data_index, avg_cpu_pctsz,
                    (unsigned long long)in.avg_cpu_pct, LIMEADE_FIELD_DELIM),
        LIMEADE_ERROR_GARBAGE);
  data_index += avg_cpu_pctsz;

  CHECK(limeade_wru(data_index, mem_total_kbsz,
                    (unsigned long long)in.mem_total_kb, LIMEADE_FIELD_DELIM),
        LIMEADE_ERROR_GARBAGE);
  data_index += mem_total_kbsz;

  CHECK(limeade_wru(data_index, mem_free_kbsz,
          (unsigned long long)in.mem_free_kb,
          LIMEADE_FIELD_DELIM),
        LIMEADE_ERROR_GARBAGE);
  data_index += mem_free_kbsz;

  CHECK(limeade_wru(data_index, mem_available_kbsz,
          (unsigned long long)in.mem_available_kb,
          LIMEADE_FIELD_DELIM),
        LIMEADE_ERROR_GARBAGE);
  data_index += mem_available_kbsz;

  CHECK(limeade_wru(data_index, mem_cached_kbsz,
          (unsigned long long)in.mem_cached_kb,
          LIMEADE_FIELD_DELIM),
        LIMEADE_ERROR_GARBAGE);
  data_index += mem_cached_kbsz;

  CHECK(limeade_wrd(data_index, load_1msz, in.load_1m, LIMEADE_FIELD_DELIM),
        LIMEADE_ERROR_GARBAGE);
  data_index += load_1msz;

  CHECK(limeade_wrd(data_index, load_5msz, in.load_5m, LIMEADE_FIELD_DELIM),
        LIMEADE_ERROR_GARBAGE);
  data_index += load_5msz;

  CHECK(limeade_wrd(data_index, load_15msz, in.load_15m, LIMEADE_FIELD_DELIM),
        LIMEADE_ERROR_GARBAGE);
  data_index += load_15msz;

  limeade_wrs(data_index, in.cores_json, LIMEADE_ROW_DELIM);

  return ret;
}

LIMEADE_DATA limeade_gendata_ask(LIMEADE_PACKET_ASK in)
{
  LIMEADE_DATA ret = {0, NULL};
  char *data_index;
  size_t is_new_devicesz, is_new_sessionsz, ts_ssz, ts_mssz;

  is_new_devicesz  = limeade_szu((unsigned long long)in.is_new_device);
  is_new_sessionsz = limeade_szu((unsigned long long)in.is_new_session);
  ts_ssz           = limeade_szu((unsigned long long)in.ts.s);
  ts_mssz          = limeade_szu((unsigned long long)in.ts.ms);

  ret.len = is_new_devicesz + is_new_sessionsz + ts_ssz + ts_mssz;

  ret.data   = malloc(ret.len);
  data_index = (char *)ret.data;

  CHECK(limeade_wru(data_index, is_new_devicesz,
                    (unsigned long long)in.is_new_device,
                    LIMEADE_FIELD_DELIM),
      LIMEADE_ERROR_GARBAGE);
  data_index += is_new_devicesz;
  
  CHECK(limeade_wru(data_index, is_new_sessionsz,
          (unsigned long long)in.is_new_session,
          LIMEADE_FIELD_DELIM),
      LIMEADE_ERROR_GARBAGE);
  data_index += is_new_sessionsz;
  
  CHECK(limeade_wru(data_index, ts_ssz, (unsigned long long)in.ts.s,
                    LIMEADE_FIELD_DELIM),
        LIMEADE_ERROR_GARBAGE);
  data_index += ts_ssz;
  
  CHECK(limeade_wru(data_index, ts_mssz, (unsigned long long)in.ts.ms,
                    LIMEADE_ROW_DELIM),
        LIMEADE_ERROR_GARBAGE);

  return ret;
}

LIMEADE_DATA limeade_gendata_answer(LIMEADE_PACKET_ANSWER in)
{
  LIMEADE_DATA ret = {0, NULL};
  ret.len          = limeade_szu((unsigned long long)(in.accepted ? 1 : 0));
  ret.data         = malloc(ret.len);
  
  CHECK(limeade_wru(ret.data, acceptedsz, (unsigned long long)(in.accepted ? 1 : 0),
                    LIMEADE_ROW_DELIM),
        LIMEADE_ERROR_GARBAGE);

  return ret;
}

LIMEADE_DATA limeade_gendata_commandeer(LIMEADE_PACKET_COMMANDEER in)
{
  LIMEADE_DATA ret = {0, NULL};
  char *data_index;
  size_t commandsz, require_ttysz, require_adminsz;

  commandsz       = limeade_szs(in.command);
  require_ttysz   = limeade_szu((unsigned long long)in.require_tty);
  require_adminsz = limeade_szu((unsigned long long)in.require_admin);

  ret.len = commandsz + require_ttysz + require_adminsz;

  ret.data = malloc(ret.len);
  data_index = (char *)ret.data;
  
  limeade_wrs(data_index, in.command, LIMEADE_FIELD_DELIM);
  data_index += commandsz;
  
  CHECK(limeade_wru(data_index, require_ttysz,
                    (unsigned long long)in.require_tty,
                    LIMEADE_FIELD_DELIM),
        LIMEADE_ERROR_GARBAGE);
  data_index += require_ttysz;

  CHECK(limeade_wru(data_index, require_adminsz,
                    (unsigned long long)in.require_admin,
                    LIMEADE_ROW_DELIM),
        LIMEADE_ERROR_GARBAGE);

  return ret;
}

// this macro is used in limeade_compile and limeade_compress
#define CHECK(expr, err)                                                       \
  if(expr)                                                                     \
  {                                                                            \
    limeade_inserror(err);                                                     \
    return ret;                                                                \
  }

LIMEADE_PACKET limeade_compile(LIMEADE_PACKET_FLAGS flags,
                               LIMEADE_SESSION session_id, void *data)
{
  /* Turns packet segments into a full packet, assumes data is accurate */
  LIMEADE_PACKET ret = {0, NULL};
  unsigned char *dst;

  // sizeof(sessionID = 5)
  ret.sz = sizeof(LIMEADE_MAGIC) + sizeof(LIMEADE_PACKET_FLAGS)
            + 5 + flags.datasz_after_compression;

  ret.data = (unsigned char *)malloc(ret.sz);
  dst = ret.data;

  memcpy(dst, &LIMEADE_MAGIC, magic_len);
  dst += magic_len;

  memcpy(dst, &flags, flags_len);
  dst += flags_len;

  memcpy(dst, session_id, 5);
  dst += session_len;

  memcpy(dst, data, data_len);

  return ret;
}

LIMEADE_DATA limeade_compress(LIMEADE_DATA *in)
{
  // TODO
  /* Wrapper for DEFLATE @ compression level 5, used for packet data */
  LIMEADE_DATA ret = {0, NULL};
  size_t cap;
  uLong src_len;
  uLongf dst_len;
  int zret;

  // sanity checks
  CHECK(in == NULL, LIMEADE_ERROR_PACKET_DATA);
  CHECK(in->data == NULL || in->len == 0, LIMEADE_ERROR_PACKET_DATA);

  src_len = (uLong)in->len;
  cap = (size_t)compressBound(src_len);
  CHECK(cap == 0, LIMEADE_ERROR_ZLIB);

  ret.data = malloc(cap);

  dst_len = (uLongf)cap;
  zret = compress2((Bytef *)ret.data, &dst_len, (const Bytef *)in->data,
                   src_len, 5);
  if (zret != Z_OK)
  {
    return limeade_fail(ret);
  }

  ret.len = (size_t)dst_len;

  CHECK(true, LIMEADE_SUCCESS);
}

// this macro is used in all of the exported functions
#define CHECK()                                                                \ 
  LIMEADE_ERROR err = limeade_poperror();                                      \
  if(err != LIMEADE_SUCCESS)                                                   \
  {                                                                            \
    limeade_inserror(err);                                                     \
    return {0, NULL};                                                          \
  }

// EXPORTED FUNCTIONS

LIMEADE_PACKET limeade_compile_sysoverv(LIMEADE_PACKET_SYSOVERV in, LIMEADE_SESSION session)
{
  LIMEADE_PACKET ret;
  LIMEADE_DATA data_pre, data_post;
  LIMEADE_PACKET_FLAGS flags;
  unsigned int datasz_pre, datasz_post;

  data_pre  = limeade_gendata_sysoverv(in);
  CHECK();
  
  data_post = limeade_compress(data_pre);
  CHECK();

  flags     = limeade_genflags(LIMEADE_PACKET_SYSOVERV, data_pre.len, data_post.len);
  CHECK();

  ret       = limeade_compile(flags, session, data_post);
  CHECK();

  free(data_pre.data);
  free(data_post.data);

  return ret;
}

LIMEADE_PACKET limeade_compile_events(LIMEADE_PACKET_EVENTS in)
{
  LIMEADE_PACKET ret;
  LIMEADE_DATA data_pre, data_post;
  LIMEADE_PACKET_FLAGS flags;
  unsigned int datasz_pre, datasz_post;

  data_pre  = limeade_gendata_events(in);
  CHECK();

  data_post = limeade_compress(data_pre);
  CHECK();

  flags     = limeade_genflags(LIMEADE_PACKET_EVENTS, data_pre.len, data_post.len);
  CHECK();

  ret       = limeade_compile(flags, session, data_post);
  CHECK();

  free(data_pre.data);
  free(data_post.data);

  return ret;
}

LIMEADE_PACKET limeade_compile_procs_generic(LIMEADE_PACKET_PROCS_GENERIC in)
{
  LIMEADE_PACKET ret;
  LIMEADE_DATA data_pre, data_post;
  LIMEADE_PACKET_FLAGS flags;
  unsigned int datasz_pre, datasz_post;

  data_pre  = limeade_gendata_procs_generic(in);
  CHECK();

  data_post = limeade_compress(data_pre);
  CHECK();

  flags     = limeade_genflags(LIMEADE_PACKET_PROCS_GENERIC, data_pre.len, data_post.len);
  CHECK();

  ret       = limeade_compile(flags, session, data_post);
  CHECK();

  free(data_pre.data);
  free(data_post.data);

  return ret;

}

LIMEADE_PACKET limeade_compile_procs_update(LIMEADE_PACKET_PROCS_UPDATE in)
{
  // NOT IMPLEMENTED
}

LIMEADE_PACKET limeade_compile_perf(LIMEADE_PACKERF_PERF in)
{
  LIMEADE_PACKET ret;
  LIMEADE_DATA data_pre, data_post;
  LIMEADE_PACKET_FLAGS flags;
  unsigned int datasz_pre, datasz_post;

  data_pre  = limeade_gendata_perf(in);
  CHECK();
  
  data_post = limeade_compress(data_pre);
  CHECK();
  
  flags     = limeade_genflags(LIMEADE_PACKET_PERF, data_pre.len, data_post.len);
  CHECK();
  
  ret       = limeade_compile(flags, session, data_post);
  CHECK();

  free(data_pre.data);
  free(data_post.data);

  return ret;
}

LIMEADE_PACKET limeade_compile_ask(LIMEADE_PACKET_ASK in)
{
  LIMEADE_PACKET ret;
  LIMEADE_DATA data_pre, data_post;
  LIMEADE_PACKET_FLAGS flags;
  unsigned int datasz_pre, datasz_post;

  data_pre  = limeade_gendata_ask(in);
  CHECK();

  data_post = limeade_compress(data_pre);
  CHECK();

  flags     = limeade_genflags(LIMEADE_PACKET_ASK, data_pre.len, data_post.len);
  CHECK();

  ret       = limeade_compile(flags, session, data_post);
  CHECK();

  free(data_pre.data);
  free(data_post.data);

  return ret;
}

LIMEADE_PACKET limeade_compile_answer(LIMEADE_PACKET_ANSWER in)
{
  LIMEADE_PACKET ret;
  LIMEADE_DATA data_pre, data_post;
  LIMEADE_PACKET_FLAGS flags;
  unsigned int datasz_pre, datasz_post;

  data_pre = limeade_gendata_answer(in);
  CHECK();

  data_post = limeade_compress(data_pre);
  CHECK();

  flags = limeade_genflags(LIMEADE_PACKET_ANSWER, data_pre.len, data_post.len);
  CHECK();

  ret = limeade_compile(flags, session, data_post);
  CHECK();

  free(data_pre.data);
  free(data_post.data);

  return ret;
}

LIMEADE_PACKET limeade_compile_commandeer(LIMEADE_PACKET_COMMANDEER in)
{
  LIMEADE_PACKET ret;
  LIMEADE_DATA data_pre, data_post;
  LIMEADE_PACKET_FLAGS flags;
  unsigned int datasz_pre, datasz_post;

  data_pre = limeade_gendata_commandeer(in);
  CHECK();

  data_post = limeade_compress(data_pre);
  CHECK();

  flags = limeade_genflags(LIMEADE_PACKET_COMMANDEER, data_pre.len, data_post.len);
  CHECK();

  ret = limeade_compile(flags, session, data_post);
  CHECK();

  free(data_pre.data);
  free(data_post.data);

  return ret;
}

// to avoid naming collisions
#undef CHECK
