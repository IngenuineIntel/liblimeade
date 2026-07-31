// diag.c
// diagnostic visualizations
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

#include <liblimeade/liblimeade.h>

static int LIMEADE_DEBUG_ENABLED = 0;
static int LIMEADE_DEBUG_VERBOSITY = 0;

void limeade_enable_debugging()
{
  LIMEADE_DEBUG_ENABLED = 1;
}

void limeade_disable_debugging()
{
  LIMEADE_DEBUG_ENABLED = 0;
}

void limeade_set_verbosity(int level)
{
  if(level >= 1 && level <= 3)
  {
    LIMEADE_DEBUG_VERBOSITY = level;
  }
}

#define DBG_OR_QUIT() if(LIMEADE_DEBUG_ENABLED == 0) return;

void limeade_diag_print_header(void)
{
  printf("\nliblimeade v%i.%i diagnostic visualization\n\n",
  LIBLIMEADE_VERSION[0], LIBLIMEADE_VERSION[1]);
}

void limeade_diag_repr_context(LIMEADE_CONTEXT ctx)
{
  DBG_OR_QUIT();

  limeade_diag_print_header();

  printf("LIMEADE_CONTEXT\nuint8_T role = ");
  if (ctx.role == LIMEADE_ROLE_CLIENT)
  {
    printf("LIMEADE_ROLE_CLIENT\n");
  } else if (ctx.role == LIMEADE_ROLE_HOST)
  {
    printf("LIMEADE_ROLE_HOST\n");
  } else
  {
    printf("%i (invalid value)\n", ctx.role);
  }

  printf("uint64_t sessionid = 0x%lX\n", ctx.sessionid);
  printf("pid_t ssh_child_pid = %i\n", ctx.ssh_child_pid);
  printf("int send_fd = %i\nint recv_fd = %i\n", ctx.send_fd, ctx.recv_fd);
}

void limeade_diag_repr_recv(LIMEADE_RECV recv)
{
  DBG_OR_QUIT();

  limeade_diag_print_header();

  printf("LIMEADE_RECV\nunsigned int type = ");

  switch(recv.type)
  {
    case LIMEADE_PACKET_ASK:
      printf("LIMEADE_PACKET_ASK");
    case LIMEADE_PACKET_ANSWER:
      printf("LIMEADE_PACKET_ANSWER");
    case LIMEADE_PACKET_EVENT:
      printf("LIMEADE_PACKET_EVENT");
    case LIMEADE_PACKET_EVENTS:
      printf("LIMEADE_PACKET_EVENTS");
    case LIMEADE_PACKET_PROC_GENERIC:
      printf("LIMEADE_PACKET_PROC_GENERIC");
    case LIMEADE_PACKET_PROC_UPDATE:
      printf("LIMEADE_PACKET_PROC_UPDATE");
    case LIMEADE_PACKET_PERF:
      printf("LIMEADE_PACKET_PERF");
    case LIMEADE_PACKET_COMMANDEER:
      printf("LIMEADE_PACKET_COMMANDEER");
    case LIMEADE_PACKET_CLOSE:
      printf("LIMEADE_PACKET_CLOSE");
    default:
      printf("%i (invalid value)\n", recv.type);
      goto next; // to avoid the next printf
  }

  printf(" (%iu)\n", recv.type);

  next:

  printf("size_t len = %lu\n", recv.len);
  printf("void *data = 0x%lx\n", (unsigned long)recv.data);
}

void limeade_diag_repr_pkt(int type, ...)
{
  DBG_OR_QUIT();

  va_list arg;
  va_start(arg, type);

  limeade_diag_print_header();

  switch(type)
  {
    case LIMEADE_PACKET_ASK:
    {
      LIMEADE_ASK data = va_arg(arg, LIMEADE_ASK);

      printf("LIMEADE_ASK\nchar *hostname = %s\n", data.hostname);
      printf("char *kernelver = %s\nchar *distro = %s\n", data.kernelver, data.distro);
      printf("char *ipaddr = %s\nchar *macaddr = %s\n", data.ipaddr, data.macaddr);
      printf("char *processor = %s\nchar*processor_vend = %s\n", data.processor, data.processor_vend);
      printf("char *ram = %s\nunsigned int response_wait_secs = %i\n", data.ram, data.response_wait_secs);
    }
    case LIMEADE_PACKET_ANSWER:
    {
      LIMEADE_ANSWER data = va_arg(arg, LIMEADE_ANSWER);

      printf("LIMEADE_ANSWER\nuint64_t sessionid = %lu", data.sessionid);
    }
    case LIMEADE_PACKET_EVENT:
    {
      LIMEADE_EVENT data = va_arg(arg, LIMEADE_EVENT);

      printf("LIMEADE_EVENT\nuint32_t ts_s = %i\nuint16_t ts_ms = %i\n", data.ts_s, data.ts_ms);
      printf("pid_t pid = %i\nchar *syscall = %s\nchar *arg1 = %s\n", data.pid, data.syscall, data.arg1);
      printf("char *arg2 = %s\nint retval = %i\n", data.arg2, data.retval);
    }

    case LIMEADE_PACKET_EVENTS:
    {
      LIMEADE_EVENTS data = va_arg(arg, LIMEADE_EVENTS);
      printf("LIMEADE_EVENTS\nuint16_t nr_events = %i\nLIMEADE_EVENT **events = {\n", data.nr_events);

      LIMEADE_EVENT *j;
      for(int i = 0; i < data.nr_events; i++)
      {
        j = data.events[i];

        printf("  {\n    uint32_t ts_s = %i\n    uint16_t ts_ms = %i\n    ", j->ts_s, j->ts_ms);
        printf("pid_t pid = %i\n    char *syscall = %s\n    char *arg1 = %s\n", j->pid, j->syscall, j->arg1);
        printf("    char *arg2 = %s\n    int retval = %i\n  },\n", j->arg2, j->retval);
      }

      printf("}\n");
    }

    case LIMEADE_PACKET_PROC_GENERIC:
    {
      LIMEADE_PROC_GENERIC data = va_arg(arg, LIMEADE_PROC_GENERIC);

      printf("LIMEADE_PROC_GENERIC\nuint32_t ts_s = %i\nuint16_t ts_ms = %i\n uint16_t total = %i",
             data.ts_s, data.ts_ms, data.total
      );

      printf("LIMEADE_PROC **procs = {\n");

      LIMEADE_PROC *j;

      for(int i = 0; i < data.total; i++)
      {
        j = data.procs[i];

        printf("  {\n");
        printf("    pid_t pid = %i\n    pid_t ppid = %i\n    uid_t uid = %i\n", j->pid, j->ppid, j->uid);
        printf("    uint16_t threads = %i\n    uint32_t cpu_ticks = %i\n", j->threads, j->cpu_ticks);
        printf("    uint32_t vm_rss_kb = %i\n    char *command = %s\n  },\n", j->vm_rss_kb, j->command);
      }
    }

    case LIMEADE_PACKET_PROC_UPDATE:
    {
      LIMEADE_PROC_UPDATE data = va_arg(arg, LIMEADE_PROC_UPDATE);

      printf("LIMEADE_PROC_UPDATE\nuint32_t ts_s = %i\nuint16_t ts_ms = %i\n", data.ts_s, data.ts_ms);
      printf("uint16_t total_altered = %i\nuint16_t total_died = %i\npid_t *died = {\n", data.total_altered, data.total_died);

      if (data.total_died != 0)
      {


        int in_loop = data.total_died / 3;
        int rem     = data.total_died % 3;

        for(int i = 0; i < in_loop; i += 3)
        {
          printf("  %i, %i, %i,\n", data.died[i], data.died[i+1], data.died[i+2]);
        }

        if (rem == 1)
        {
          printf("  %i,\n}\n", data.died[in_loop]);
        } else if (rem == 2)
        {
          printf("  %i, %i\n}\n", data.died[in_loop], data.died[in_loop+1]);
        } // else // rem = 0
      }

      if (data.total_altered != 0)
      {
        printf("LIMEADE_PROC **altered = {\n");

        LIMEADE_PROC *j;

        for(int i = 0; i < data.total_altered; i++)
        {
          j = data.altered[i];

          printf("  {\n");
          printf("    pid_t pid = %i\n    pid_t ppid = %i\n    uid_t uid = %i\n", j->pid, j->ppid, j->uid);
          printf("    uint16_t threads = %i\n    uint32_t cpu_ticks = %i\n", j->threads, j->cpu_ticks);
          printf("    uint32_t vm_rss_kb = %i\n    char *command = %s\n  },\n", j->vm_rss_kb, j->command);
        }
      }
    }

    case LIMEADE_PACKET_PERF:
    {
      LIMEADE_PERF data = va_arg(arg, LIMEADE_PERF);
      printf("LIMEADE_PERF\nuint32_t ts_s = %i\nuint16_t ts_ms = %i\n", data.ts_s, data.ts_ms);
      printf("uint8_t cores = %i\nuint32_t avg_cpu_pct = %i\n", data.cores, data.avg_cpu_pct);
      printf("uint32_t mem_total_kb = %i\nuint32_t mem_free_kb = %i", data.mem_total_kb, data.mem_free_kb);
      printf("uint32_t mem_available_kb = %i\nuint32_t mem_cached_kb = %i\n", data.mem_available_kb, data.mem_cached_kb);
      printf("double load_1m = %f\ndouble load_5m = %f\ndouble load_15m = %f", data.load_1m, data.load_5m, data.load_15m);
      printf("\nchar *cores_json = %s\n", data.cores_json);
    }

    case LIMEADE_PACKET_COMMANDEER:
    {
      LIMEADE_COMMANDEER data = va_arg(arg, LIMEADE_COMMANDEER);

      printf("LIMEADE_COMMANDEER\nchar *command = %s\nuint8_t exec_with_root = %i", data.command, data.exec_with_root);
      printf("\nuint8_t exec_with_tty = %i\nuint8_t exec_with_jail = %i\n", data.exec_with_tty, data.exec_with_jail);
    }

    case LIMEADE_PACKET_CLOSE:
      printf("LIMEADE_CLOSE\nempty\n");

    default:
      printf("%i (invalid value)\n", type);
  }
}

static char *limeade_diag_repr_error(LIMEADE_ERROR err)
{
  switch(err)
  {
    case LIMEADE_SUCCESS:
      return "SUCCESS";
    case LIMEADE_ERROR_PACKET_MAGIC:
      return "Bad Magic";
    case LIMEADE_ERROR_PACKET_FORMAT:
      return "Bad Format";
    case LIMEADE_ERROR_PACKET_SIZE:
      return "Bad Size";
    case LIMEADE_ERROR_PACKET_SESSION:
      return "Bad Session ID";
    case LIMEADE_ERROR_PACKET_DATA:
      return "Bad Packet Data";
    case LIMEADE_ERROR_PACKET_COMPRESSED:
      return "Bad Compression";
    case LIMEADE_ERROR_DECODING_FATAL:
      return "Fatal Decoding Error";
    case LIMEADE_ERROR_DECODING_NONFATAL:
      return "Non-Fatal Decoding Error";
    case LIMEADE_ERROR_GARBAGE:
      return "Garbage Argument";
    case LIMEADE_ERROR_SSH_PROC:
      return "SSH Tunneling Child Error";
    case LIMEADE_ERROR_INVALID_CONTEXT:
      return "Garbage Context";
    case LIMEADE_ERROR_MEMORY:
      return "Bad Allocation";
    case LIMEADE_ERROR_OTHER:
      return "Other";
    default:
      return "invalid";
  }
}

void limeade_diag_repr_errors(void)
{
  DBG_OR_QUIT();

  limeade_diag_print_header();

  printf("Error Buffer size: %i\n", LIMEADE_ERROR_BUFFER_SIZE);
  printf("Current index: %i\n", LIMEADE_ERROR_INDEX);

  for(int i = 0; i < LIMEADE_ERROR_INDEX; i++)
  {
    printf("%s (%i)\n", limeade_diag_repr_error(LIMEADE_ERRORS[i]), LIMEADE_ERRORS[i]);
  }

}

#undef DBG_OR_QUIT
