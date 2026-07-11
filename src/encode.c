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

#include<stdlib.h>
#include<string.h>
#include<stdio.h>
#include<stdarg.h>

#include<liblimeade.h>

typedef struct
{
  size_t len;
  void *data;
} LIMEADE_DATA;

#define LIMEADE_RETURN_RET_IF_NEG_N(n, ret) do { if ((n) < 0) { return (ret); } } while (0)

// START HELPER FUNCTIONS

LIMEADE_PACKET_FLAGS limeade_genflags(LIMEADE_PACKET_TYPE type,
                                      unsigned int datasz_before_compression,
                                      unsigned int datasz_after_compression)
{
  return (LIMEADE_PACKET_FLAGS)
  {
    .version                   = LIMEADE_PROTOCOL_VERSION,
    .type                      = type,
    .datasz_before_compression = (datasz_before_compression < 0b11111111111111) ? datasz_before_compression : 0,
    .datasz_after_compression  = (datasz_after_compression  < 0b11111111111111) ? datasz_after_compression  : 0,
    .field_delim               = LIMEADE_FIELD_DELIM,
    .row_delim                 = LIMEADE_ROW_DELIM
  };
}

char *limeade_prep_str(char *in)
{
  char *ret;
  size_t len;
  size_t i;

  if (in == NULL)
  {
    in = "";
  }

  len = strlen(in);
  ret = malloc(len + 1);
  if (ret == NULL)
  {
    return NULL;
  }

  memcpy(ret, in, len + 1);

  for (i = 0; i < len; i++)
  {
    if (ret[i] == LIMEADE_FIELD_DELIM || ret[i] == LIMEADE_ROW_DELIM)
    {
      ret[i] = ' ';
    }
  }

  return ret;
}

char *limeade_prep_int(int in)
{
  char ret[12]; // len(str(2**31))+2 (+1 for negative sign, +1 for \0)
  snprintf(ret, sizeof(ret), "%d", in);
  return limeade_prep_str(ret);
}

char *limeade_prep_double(double in)
{
  char ret[32];
  snprintf(ret, sizeof(ret), "%.17g", in);
  return limeade_prep_str(ret);
}

LIMEADE_DATA limeade_gendata_sysoverv(LIMEADE_PACKET_SYSOVERV in)
{
  LIMEADE_DATA ret = {0, NULL};
  char *hostname, *kernelver, *distro, *ipaddr, *macaddr, *processor, *processor_vend;
  char *ram_gbs, *data_index, *scan;
  size_t hostnamesz, kernelversz, distrosz, ipaddrsz, macaddrsz, processorsz;
  size_t processor_vendsz, ram_gbssz;

  hostname       = limeade_prep_str(in.hostname);
  kernelver      = limeade_prep_str(in.kernelver);
  distro         = limeade_prep_str(in.distro);
  ipaddr         = limeade_prep_str(in.ipaddr);
  macaddr        = limeade_prep_str(in.macaddr);
  processor      = limeade_prep_str(in.processor);
  processor_vend = limeade_prep_str(in.processor_vend);
  ram_gbs        = limeade_prep_int(in.ram_gbs);

  hostnamesz       = strlen(hostname) + 1;
  kernelversz      = strlen(kernelver) + 1;
  distrosz         = strlen(distro) + 1;
  ipaddrsz         = strlen(ipaddr) + 1;
  macaddrsz        = strlen(macaddr) + 1;
  processorsz      = strlen(processor) + 1;
  processor_vendsz = strlen(processor_vend) + 1;
  ram_gbssz        = strlen(ram_gbs) + 1;

  ret.len = hostnamesz + kernelversz + distrosz + ipaddrsz + macaddrsz;
  ret.len += processorsz + processor_vendsz + ram_gbssz;

  ret.data = malloc(ret.len);
  if (ret.data == NULL)
  {
    free(hostname);
    free(kernelver);
    free(distro);
    free(ipaddr);
    free(macaddr);
    free(processor);
    free(processor_vend);
    free(ram_gbs);
    ret.len = 0;
    return ret;
  }

  data_index = (char *)ret.data;

  strcpy(data_index, hostname);
  data_index += hostnamesz;
  strcpy(data_index, kernelver);
  data_index += kernelversz;
  strcpy(data_index, distro);
  data_index += distrosz;
  strcpy(data_index, ipaddr);
  data_index += ipaddrsz;
  strcpy(data_index, macaddr);
  data_index += macaddrsz;
  strcpy(data_index, processor);
  data_index += processorsz;
  strcpy(data_index, processor_vend);
  data_index += processor_vendsz;
  strcpy(data_index, ram_gbs);

  for (scan = (char *)ret.data; scan < (char *)ret.data + ret.len; scan++)
  {
    if (*scan == '\0')
    {
      *scan = LIMEADE_FIELD_DELIM;
    }
  }
  ((char *)ret.data)[ret.len - 1] = LIMEADE_ROW_DELIM;

  free(hostname);
  free(kernelver);
  free(distro);
  free(ipaddr);
  free(macaddr);
  free(processor);
  free(processor_vend);
  free(ram_gbs);

  return ret;
}

LIMEADE_DATA limeade_gendata_events(LIMEADE_PACKET_EVENTS in) 
{
  LIMEADE_DATA ret = {0, NULL};
  char *ts_s, *ts_ms, *pid, *type, *subtype, *arg1, *arg2, *retval, *data_index, *scan;
  size_t ts_ssz, ts_mssz, pidsz, typesz, subtypesz, arg1sz, arg2sz, retvalsz, i;
  size_t nr_events;
  int n;

  if (in.events == NULL)
  {
    return ret;
  }

  nr_events = in.nr_events;

  for(i = 0; i < nr_events; i++)
  {
    EVENT *ev;

    if (in.events[i] == NULL || *in.events[i] == NULL)
    {
      return ret;
    }

    ev = *in.events[i];
    n = snprintf(NULL, 0, "%d", (int)ev->ts.s);
    LIMEADE_RETURN_RET_IF_NEG_N(n, ret);
    ts_ssz = (size_t)n + 1;

    n = snprintf(NULL, 0, "%d", (int)ev->ts.ms);
    LIMEADE_RETURN_RET_IF_NEG_N(n, ret);
    ts_mssz = (size_t)n + 1;

    n = snprintf(NULL, 0, "%d", ev->pid);
    LIMEADE_RETURN_RET_IF_NEG_N(n, ret);
    pidsz = (size_t)n + 1;

    typesz    = strlen((ev->type != NULL)    ? ev->type    : "") + 1;
    subtypesz = strlen((ev->subtype != NULL) ? ev->subtype : "") + 1;
    arg1sz    = strlen((ev->arg1 != NULL)    ? ev->arg1    : "") + 1;
    arg2sz    = strlen((ev->arg2 != NULL)    ? ev->arg2    : "") + 1;

    n = snprintf(NULL, 0, "%d", ev->retval);
    LIMEADE_RETURN_RET_IF_NEG_N(n, ret);
    retvalsz = (size_t)n + 1;

    ret.len += ts_ssz + ts_mssz + pidsz + typesz + subtypesz + arg1sz + arg2sz + retvalsz;
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

  for(i = 0; i < nr_events; i++)
  {
    EVENT *ev = *in.events[i];

    ts_s    = limeade_prep_int(ev->ts.s);
    ts_ms   = limeade_prep_int(ev->ts.ms);
    pid     = limeade_prep_int(ev->pid);
    type    = limeade_prep_str(ev->type);
    subtype = limeade_prep_str(ev->subtype);
    arg1    = limeade_prep_str(ev->arg1);
    arg2    = limeade_prep_str(ev->arg2);
    retval  = limeade_prep_int(ev->retval);

    ts_ssz    = strlen(ts_s) + 1;
    ts_mssz   = strlen(ts_ms) + 1;
    pidsz     = strlen(pid) + 1;
    typesz    = strlen(type) + 1;
    subtypesz = strlen(subtype) + 1;
    arg1sz    = strlen(arg1) + 1;
    arg2sz    = strlen(arg2) + 1;
    retvalsz  = strlen(retval) + 1;

    strcpy(data_index, ts_s);
    data_index += ts_ssz;
    strcpy(data_index, ts_ms);
    data_index += ts_mssz;
    strcpy(data_index, pid);
    data_index += pidsz;
    strcpy(data_index, type);
    data_index += typesz;
    strcpy(data_index, subtype);
    data_index += subtypesz;
    strcpy(data_index, arg1);
    data_index += arg1sz;
    strcpy(data_index, arg2);
    data_index += arg2sz;
    strcpy(data_index, retval);
    data_index += retvalsz;

    *(data_index - 1) = LIMEADE_ROW_DELIM;

    free(ts_s);
    free(ts_ms);
    free(pid);
    free(type);
    free(subtype);
    free(arg1);
    free(arg2);
    free(retval);
  }

  for (scan = (char *)ret.data; scan < (char *)ret.data + ret.len; scan++)
  {
    if (*scan == '\0')
    {
      *scan = LIMEADE_FIELD_DELIM;
    }
  }

  return ret;
}

LIMEADE_DATA limeade_gendata_procs_generic(LIMEADE_PACKET_PROCS_GENERIC in)
{
  LIMEADE_DATA ret = {0, NULL};
  char *ts_s, *ts_ms, *pid, *ppid, *uid, *threads, *cpu_ticks, *vm_rss_kb, *comm;
  char *data_index, *scan;
  size_t ts_ssz, ts_mssz, pidsz, ppidsz, uidsz, threadssz, cpu_tickssz, vm_rss_kbsz, commsz, i;
  size_t nr_processes;
  int n;

  if (in.processes == NULL)
  {
    return ret;
  }

  n = snprintf(NULL, 0, "%d", (int)in.ts.s);
  LIMEADE_RETURN_RET_IF_NEG_N(n, ret);
  ts_ssz = (size_t)n + 1;

  n = snprintf(NULL, 0, "%d", (int)in.ts.ms);
  LIMEADE_RETURN_RET_IF_NEG_N(n, ret);
  ts_mssz = (size_t)n + 1;

  nr_processes = in.nr_processes;

  for (i = 0; i < nr_processes; i++)
  {
    LIMEADE_PACKET_PROC *proc;

    if (in.processes[i] == NULL)
    {
      return ret;
    }

    proc = (LIMEADE_PACKET_PROC *)in.processes[i];

    n = snprintf(NULL, 0, "%d", (int)proc->pid);
    LIMEADE_RETURN_RET_IF_NEG_N(n, ret);
    pidsz = (size_t)n + 1;

    n = snprintf(NULL, 0, "%d", (int)proc->ppid);
    LIMEADE_RETURN_RET_IF_NEG_N(n, ret);
    ppidsz = (size_t)n + 1;

    n = snprintf(NULL, 0, "%u", (unsigned int)proc->uid);
    LIMEADE_RETURN_RET_IF_NEG_N(n, ret);
    uidsz = (size_t)n + 1;

    n = snprintf(NULL, 0, "%u", (unsigned int)proc->threads);
    LIMEADE_RETURN_RET_IF_NEG_N(n, ret);
    threadssz = (size_t)n + 1;

    n = snprintf(NULL, 0, "%u", (unsigned int)proc->cpu_ticks);
    LIMEADE_RETURN_RET_IF_NEG_N(n, ret);
    cpu_tickssz = (size_t)n + 1;

    n = snprintf(NULL, 0, "%u", (unsigned int)proc->vm_rss_kb);
    LIMEADE_RETURN_RET_IF_NEG_N(n, ret);
    vm_rss_kbsz = (size_t)n + 1;

    commsz = strlen((proc->comm != NULL) ? proc->comm : "") + 1;

    ret.len += ts_ssz + ts_mssz + pidsz + ppidsz + uidsz + threadssz + cpu_tickssz + vm_rss_kbsz + commsz;
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

    ts_s      = limeade_prep_int(in.ts.s);
    ts_ms     = limeade_prep_int(in.ts.ms);
    pid       = limeade_prep_int(proc->pid);
    ppid      = limeade_prep_int(proc->ppid);
    uid       = limeade_prep_int(proc->uid);
    threads   = limeade_prep_int(proc->threads);
    cpu_ticks = limeade_prep_int(proc->cpu_ticks);
    vm_rss_kb = limeade_prep_int(proc->vm_rss_kb);
    comm      = limeade_prep_str(proc->comm);

    ts_ssz      = strlen(ts_s) + 1;
    ts_mssz     = strlen(ts_ms) + 1;
    pidsz       = strlen(pid) + 1;
    ppidsz      = strlen(ppid) + 1;
    uidsz       = strlen(uid) + 1;
    threadssz   = strlen(threads) + 1;
    cpu_tickssz = strlen(cpu_ticks) + 1;
    vm_rss_kbsz = strlen(vm_rss_kb) + 1;
    commsz      = strlen(comm) + 1;

    strcpy(data_index, ts_s);
    data_index += ts_ssz;
    strcpy(data_index, ts_ms);
    data_index += ts_mssz;
    strcpy(data_index, pid);
    data_index += pidsz;
    strcpy(data_index, ppid);
    data_index += ppidsz;
    strcpy(data_index, uid);
    data_index += uidsz;
    strcpy(data_index, threads);
    data_index += threadssz;
    strcpy(data_index, cpu_ticks);
    data_index += cpu_tickssz;
    strcpy(data_index, vm_rss_kb);
    data_index += vm_rss_kbsz;
    strcpy(data_index, comm);
    data_index += commsz;

    *(data_index - 1) = LIMEADE_ROW_DELIM;

    free(ts_s);
    free(ts_ms);
    free(pid);
    free(ppid);
    free(uid);
    free(threads);
    free(cpu_ticks);
    free(vm_rss_kb);
    free(comm);
  }

  for (scan = (char *)ret.data; scan < (char *)ret.data + ret.len; scan++)
  {
    if (*scan == '\0')
    {
      *scan = LIMEADE_FIELD_DELIM;
    }
  }

  return ret;
}

LIMEADE_DATA limeade_gendata_procs_update(LIMEADE_PACKET_PROCS_UPDATE in) // NOT IMPLEMENTED
{
  // TODO
}

LIMEADE_DATA limeade_gendata_perf(LIMEADE_PACKET_PERF in)
{
  LIMEADE_DATA ret = {0, NULL};
  char *ts_s, *ts_ms, *cores, *avg_cpu_pct, *mem_total_kb, *mem_free_kb, *mem_available_kb;
  char *mem_cached_kb, *load_1m, *load_5m, *load_15m, *cores_json, *data_index, *scan;
  size_t ts_ssz, ts_mssz, coressz, avg_cpu_pctsz, mem_total_kbsz, mem_free_kbsz;
  size_t mem_available_kbsz, mem_cached_kbsz, load_1msz, load_5msz, load_15msz, cores_jsonsz;

  ts_s             = limeade_prep_int(in.ts.s);
  ts_ms            = limeade_prep_int(in.ts.ms);
  cores            = limeade_prep_int(in.cores);
  avg_cpu_pct      = limeade_prep_int(in.avg_cpu_pct);
  mem_total_kb     = limeade_prep_int(in.mem_total_kb);
  mem_free_kb      = limeade_prep_int(in.mem_free_kb);
  mem_available_kb = limeade_prep_int(in.mem_available_kb);
  mem_cached_kb    = limeade_prep_int(in.mem_cached_kb);
  load_1m          = limeade_prep_double(in.load_1m);
  load_5m          = limeade_prep_double(in.load_5m);
  load_15m         = limeade_prep_double(in.load_15m);
  cores_json       = limeade_prep_str(in.cores_json);

  ts_ssz           = strlen(ts_s) + 1;
  ts_mssz          = strlen(ts_ms) + 1;
  coressz          = strlen(cores) + 1;
  avg_cpu_pctsz    = strlen(avg_cpu_pct) + 1;
  mem_total_kbsz   = strlen(mem_total_kb) + 1;
  mem_free_kbsz    = strlen(mem_free_kb) + 1;
  mem_available_kbsz = strlen(mem_available_kb) + 1;
  mem_cached_kbsz  = strlen(mem_cached_kb) + 1;
  load_1msz        = strlen(load_1m) + 1;
  load_5msz        = strlen(load_5m) + 1;
  load_15msz       = strlen(load_15m) + 1;
  cores_jsonsz     = strlen(cores_json) + 1;

  ret.len = ts_ssz + ts_mssz + coressz + avg_cpu_pctsz + mem_total_kbsz + mem_free_kbsz;
  ret.len += mem_available_kbsz + mem_cached_kbsz + load_1msz + load_5msz + load_15msz + cores_jsonsz;

  ret.data = malloc(ret.len);
  if (ret.data == NULL)
  {
    ret.len = 0;
    goto end;
  }

  data_index = (char *)ret.data;

  strcpy(data_index, ts_s);
  data_index += ts_ssz;
  strcpy(data_index, ts_ms);
  data_index += ts_mssz;
  strcpy(data_index, cores);
  data_index += coressz;
  strcpy(data_index, avg_cpu_pct);
  data_index += avg_cpu_pctsz;
  strcpy(data_index, mem_total_kb);
  data_index += mem_total_kbsz;
  strcpy(data_index, mem_free_kb);
  data_index += mem_free_kbsz;
  strcpy(data_index, mem_available_kb);
  data_index += mem_available_kbsz;
  strcpy(data_index, mem_cached_kb);
  data_index += mem_cached_kbsz;
  strcpy(data_index, load_1m);
  data_index += load_1msz;
  strcpy(data_index, load_5m);
  data_index += load_5msz;
  strcpy(data_index, load_15m);
  data_index += load_15msz;
  strcpy(data_index, cores_json);

  for (scan = (char *)ret.data; scan < (char *)ret.data + ret.len; scan++)
  {
    if (*scan == '\0')
    {
      *scan = LIMEADE_FIELD_DELIM;
    }
  }
  ((char *)ret.data)[ret.len - 1] = LIMEADE_ROW_DELIM;

  end:
  free(ts_s);
  free(ts_ms);
  free(cores);
  free(avg_cpu_pct);
  free(mem_total_kb);
  free(mem_free_kb);
  free(mem_available_kb);
  free(mem_cached_kb);
  free(load_1m);
  free(load_5m);
  free(load_15m);
  free(cores_json);

  return ret; // TODO compression
}

LIMEADE_DATA limeade_gendata_ask(LIMEADE_PACKET_ASK in)
{
  LIMEADE_DATA ret = {0, NULL};
  char *is_new_device, *is_new_session, *ts, *data_index, *i;
  size_t is_new_devicesz, is_new_sessionsz, tssz;

  is_new_device  = limeade_prep_int(in.is_new_device);
  is_new_session = limeade_prep_int(in.is_new_session);
  ts             = limeade_prep_int(in.ts);

  is_new_devicesz  = strlen(is_new_device) + 1;
  is_new_sessionsz = strlen(is_new_session) + 1;
  tssz             = strlen(ts) + 1;

  ret.len = is_new_devicesz + is_new_sessionsz + tssz;

  ret.data = malloc(ret.len);
  if(ret.data == NULL)
  {
    ret.len = 0;
    goto end;
  }

  data_index = (char*)ret.data;
  strcpy(data_index, is_new_device);
  data_index += is_new_devicesz;
  strcpy(data_index, is_new_session);
  data_index += is_new_sessionsz;
  strcpy(data_index, tssz);


  for (i = (char *)ret.data; i < (char *)ret.data + ret.len; i++)
  {
    if (*i == '\0')
    {
      *i = LIMEADE_FIELD_DELIM;
    }
  }
  ((char *)ret.data)[ret.len - 1] = LIMEADE_ROW_DELIM;

  end:
  free(is_new_device);
  free(is_new_session);
  free(ts);
  return ret;
}

LIMEADE_DATA limeade_gendata_answer(LIMEADE_PACKET_ANSWER in)
{
  LIMEADE_DATA ret = {0, NULL};
  // TODO
}

LIMEADE_DATA limeade_gendata_commandeer(LIMEADE_PACKET_COMMANDEER in)
{
  LIMEADE_DATA ret = {0, NULL};
  char *port, *user, *passwd, *admin, *data_index, *scan;
  size_t portsz, usersz, passwdsz, adminsz;

  port   = limeade_prep_int(in.port);
  user   = limeade_prep_str(in.user);
  passwd = limeade_prep_str(in.passwd);
  admin  = limeade_prep_str(in.admin);

  portsz   = strlen(port) + 1;
  usersz   = strlen(user) + 1;
  passwdsz = strlen(passwd) + 1;
  admin    = strlen(admin) + 1;

  ret.len = portsz + usersz + passwdsz + adminsz;

  ret.data = malloc(ret.len);
  ret(ret.data == NULL)
  {
    ret.len = 0;
    goto end;
  }

  data_index = (char*)ret.data;
  strcpy(data_index, request_id);
  data_index += request_idsz;
  strcpy(data_index, command);
  data_index += commandsz;
  strcpy(data_index, args_json);
  data_index += args_jsonsz;
  strcpy(data_index, require_tty);
  data_index += require_ttysz;
  strcpy(data_index, require_admin);
  data_index += require_adminsz;
  strcpy(data_index, timeout_s);

  for (scan = (char *)ret.data; scan < (char *)ret.data + ret.len; scan++)
  {
    if (*scan == '\0')
    {
      *scan = LIMEADE_FIELD_DELIM;
    }
  }
  ((char *)ret.data)[ret.len - 1] = LIMEADE_ROW_DELIM;

  end:
  free(request_id);
  free(command);
  free(args_json);
  free(require_tty);
  free(require_admin);
  free(timeout_s);
  return ret;

}



LIMEADE_PACKET limeade_compile(char *magic, LIMEADE_PACKET_FLAGS flags,
                               LIMEADE_SESSION session_id, void *data)
{
  // TODO
}

// END HELPER FUNCTIONS

LIMEADE_PACKET limeade_compile_sysoverv(LIMEADE_PACKET_SYSOVERV in)
{
  // TODO
}

LIMEADE_PACKET limeade_compile_events(LIMEADE_PACKET_EVENTS in)
{
  // TODO
}

LIMEADE_PACKET limeade_compile_procs_generic(LIMEADE_PACKET_PROCS_GENERIC in)
{
  // TODO
}

LIMEADE_PACKET limeade_compile_procs_update(LIMEADE_PACKET_PROCS_UPDATE in) // NOT IMPLEMENTED
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

