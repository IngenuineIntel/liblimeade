// liblimeade.h
// entry point header file for liblimeade
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

#ifndef _LIBLIMEADE_ENTRY_H
#define _LIBLIMEADE_ENTRY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

/*** INIT ***/

// context manager for library usage (the "this|self|me" of the library)
typedef struct LIMEADE_CONTEXT_IMPL *LIMEADE_CONTEXT;

// representation of a raw payload
typedef struct
{
  uint32_t sz;         // payload size in bytes
  unsigned char *data; // payload bytes
} LIMEADE_PACKET;

// initialization functions for host and client
LIMEADE_CONTEXT limeade_host_init(unsigned int port);
LIMEADE_CONTEXT limeade_client_init(unsigned int port, const char *user,
                                    const char *passwd);

// send functions for host and client
int limeade_host_send(LIMEADE_CONTEXT ctx, LIMEADE_PACKET pckt);
int limeade_client_send(LIMEADE_CONTEXT ctx, LIMEADE_PACKET pckt);

// receive functions for host and client
LIMEADE_PACKET limeade_host_recv(LIMEADE_CONTEXT ctx);
LIMEADE_PACKET limeade_client_recv(LIMEADE_CONTEXT ctx);

// sets timeout when waiting for complete packet
int limeade_context_set_timeout(LIMEADE_CONTEXT ctx, int timeout_ms);

// deinitialization functions for host and client
void limeade_host_free(LIMEADE_CONTEXT ctx);
void limeade_client_free(LIMEADE_CONTEXT ctx);

/***  PACKET DESIGN  ***/

// all packets have 4 components:
/// 1. the magic, which indicates the protocol
static const char LIMEADE_MAGIC[7] = "!LIME!\x00";

// clang-format off
// if on the graph below gets mangled

/// 2. the flags
// The flags are, in total, always 7 bytes, as follows:
//
//    Protocol version (major number)     - 4 bits (max 15)
//    |   Protocol version (minor number) - 4 bits (max 15)
//    |   |   Packet type                 - 4 bits (max 15)
//    |   |   |             Data size (before compression)              - 14 bits (max 16384)
//    |   |   |             |             Data size (after compression) - 14 bits (max 16384)
//    |   |   |             |             |       Field delimiter       - 1 byte
//    |   |   |             |             |       |       Row delimiter - 1 byte
//    |   |   |             |             |       |       |
// ,--+,--+,--+,------------+,------------+,------+,------+
// 0001000001010000010100110101000011101101001111111011111111

/// clang-format on

// note: all `Data`s are compression with the DEFLATE algorithm @
// compression_level 5

// version info
typedef struct
{
  uint8_t maj : 4; // major version number
  uint8_t min : 4; // minor version number
} LIBLIMEADE_VERSION;

LIBLIMEADE_VERSION LIMEADE_PROTOCOL_VERSION = {0, 1};

// packet types
typedef enum
{
  // client packets
  LIMEADE_CLIENT_SYSOVERV,      // basic system info
  LIMEADE_CLIENT_EVENTS,        // syscalls
  LIMEADE_CLIENT_PROCS_GENERIC, // process table info
  LIMEADE_CLIENT_PROCS_UPDATE,  // process table info (dynamic)
  LIMEADE_CLIENT_PERF,          // resource usage info
  LIMEADE_CLIENT_ASK,           // ask to connect
                      // host packets
  LIMEADE_HOST_ANSWER,     // confirm connection (or disband connection)
  LIMEADE_HOST_COMMANDEER, // execute command on the client
  LIMEADE_HOST_GETSHELL    // demands a cryptcat instance on the client
} LIMEADE_PACKET_TYPE;

// field delimiter
static char LIMEADE_FIELD_DELIM = '\xFE';
static char LIMEADE_ROW_DELIM = '\xFF';

// the flags themselves
typedef struct
{
  uint8_t version;
  LIMEADE_PACKET_TYPE type : 4;
  uint16_t datasz_before_compression : 14;
  uint16_t datasz_after_compression : 14;
  char field_delim;
  char row_delim;
} LIMEADE_PACKET_FLAGS;

/// 3. session id
// handed out by the host (via HOST_ANSWER)
// pseudorandomly generated, always 5 bytes
// included in both client and host packets except in CLIENT_ASK packets where
// the client has never connected before or otherwise lacks a session id
typedef char LIMEADE_SESSION[5];

/// 4. data
// takes various forms, depending on packet type
//  - generally visualized as a table, with fields and rows separated with
//    LIMEADE_FIELD_DELIM and LIMEADE_ROW_DEMLIM
//  - strings do not have nullbytes at the end

// timestamps
typedef struct
{
  uint32_t s;
  uint16_t ms;
} TS;

// CLINET_SYSOVERV
typedef struct
{
  char *hostname;       // the hostname
  char *kernelver;      // the Linux Kernel version
  char *distro;         // the Linux distribution
  char *ipaddr;         // IP address
  char *macaddr;        // MAC address
  char *processor;      // CPU type
  char *processor_vend; // the CPU's VendorID
  uint8_t ram_gbs;      // no. gigabytes of RAM
} LIMEADE_PACKET_SYSOVERV;

// CLIENT_EVENTS
typedef struct
{
  TS ts;
  pid_t pid;     // PID of the calling process
  char *type;    // type of syscall    (open, get)
  char *subtype; // subtype of syscall (opensysat2, geteuid)
  char *arg1;    // RDI at call time ("/dev/null", etc.)
  char *arg2;    // RSI at call time (b00010010, etc.)
  int retval;    // return value from syscall
} EVENT;

typedef struct
{
  uint16_t nr_events;
  EVENT **events[]; // TODO this can't be right..?
} LIMEADE_PACKET_EVENTS;

// CLIENT_PROCS_GENERIC
typedef struct
{
  pid_t pid;
  pid_t ppid;
  uid_t uid;
  uint16_t threads;   // no. threads
  uint32_t cpu_ticks; // CPU usage
  uint32_t vm_rss_kb; // memory usage (in KB)
  char *comm;         // command
} LIMEADE_PACKET_PROC;

typedef struct
{
  TS ts;
  uint16_t nr_processes;
  LIMEADE_PACKET_PROC *processes[];
} LIMEADE_PACKET_PROCS_GENERIC;

// CLIENT_PROCS_UPDATE
typedef struct LIMEADE_PACKET_PROCS_UPDATE
    LIMEADE_PACKET_PROCS_UPDATE; // NOT IMPLEMENTED

typedef struct
{
  TS ts;
  uint8_t cores;
  uint32_t avg_cpu_pct;
  uint32_t mem_total_kb;
  uint32_t mem_free_kb;
  uint32_t mem_available_kb;
  uint32_t mem_cached_kb;
  double load_1m;
  double load_5m;
  double load_15m;
  char *cores_json;
} LIMEADE_PACKET_PERF;

typedef struct
{
  uint8_t is_new_device;  // if this device has connected before
  uint8_t is_new_session; // if this is a new running instance of the client
  TS ts; // timestamp (s and ms) of send time (so the host can infer latency)
} LIMEADE_PACKET_ASK;

typedef struct
{
  bool accepted;
} LIMEADE_PACKET_ANSWER;

typedef struct
{
  char *command;         // executable/command to run on the client (+args)
  uint8_t require_tty;   // if command needs a pty-like execution context
  uint8_t require_admin; // if elevated permissions are required
} LIMEADE_PACKET_COMMANDEER;

typedef struct LIMEADE_PACKET_GETSHELL LIMEADE_PACKET_GETSHELL; // TODO

/*** ENCODING ***/

LIMEADE_PACKET limeade_compile_sysoverv(LIMEADE_PACKET_SYSOVERV in);
LIMEADE_PACKET limeade_compile_events(LIMEADE_PACKET_EVENTS in);
LIMEADE_PACKET limeade_compile_procs_generic(LIMEADE_PACKET_PROCS_GENERIC in);
LIMEADE_PACKET limeade_compile_procs_update(LIMEADE_PACKET_PROCS_UPDATE in); // NOT IMPLEMENTED
LIMEADE_PACKET limeade_compile_perf(LIMEADE_PACKET_PERF in);
LIMEADE_PACKET limeade_compile_ask(LIMEADE_PACKET_ASK in);
LIMEADE_PACKET limeade_compile_answer(LIMEADE_PACKET_ANSWER in);
LIMEADE_PACKET limeade_compile_commandeer(LIMEADE_PACKET_COMMANDEER in);
LIMEADE_PACKET limeade_compile_getshell(LIMEADE_PACKET_GETSHELL in);

/***  DECODING  ***/

// parsing flags
LIMEADE_PACKET_FLAGS limeade_parseflags(char *flags_raw);

// TODO

/***  ERRORS  ***/

// since errors can't always be indicated via return values (because not all
// errors are completely fatal, so the actual return values tend to still be
// useful) they are instead indicated via an internal buffer that stores the
// last 5 errors (orTuccesses)

// not all functions insert into this buffer, but most do
// the origin of the error is generally indicated by its name

// all types of errors
typedef enum
{
  LIMEADE_SUCCESS,
  LIMEADE_ERROR_INVALID_CONTEXT,
  LIMEADE_ERROR_OOM,
  LIMEADE_ERROR_SSH_INIT,
  LIMEADE_ERROR_SSH_BIND,
  LIMEADE_ERROR_SSH_LISTEN,
  LIMEADE_ERROR_SSH_ACCEPT,
  LIMEADE_ERROR_SSH_KEX,
  LIMEADE_ERROR_SSH_AUTH,
  LIMEADE_ERROR_SSH_CHANNEL,
  LIMEADE_ERROR_SSH_SUBSYSTEM,
  LIMEADE_ERROR_SSH_IO,
  LIMEADE_ERROR_TIMEOUT,
  LIMEADE_ERROR_PACKET_MAGIC,
  LIMEADE_ERROR_PACKET_FORMAT,
  LIMEADE_ERROR_PACKET_SIZE,
  LIMEADE_ERROR_PACKET_SESSION
} LIMEADE_ERROR_TYPE;

static uint8_t LIMEADE_ERRORS[5];

// which index in the buffer is the latest error
// if =1, the latest error is at [1], the previous [0], and the next previous
// [5], etc.
static uint8_t LIMEADE_ERROR_INDEX;

// errors are added by a handler function,
void limeade_inserror(LIMEADE_ERROR_TYPE error);

// and read (popped, actually) by another handler function
LIMEADE_ERROR_TYPE limeade_poperror(void);

/***  VERSIONS  ***/

// packet versions have to be checked for compatibility, as (inevitably) not
// every version of the protocol will be compatible with others
//
// generally, every version tries to be compatible with the previous major
// version (a client won't send a CLIENT_PROCS_UPDATE if the previous major
// version doesn't support it, etc.), unless:
//  - it just can't be done, for whatever reason
//  - the previous major version is 0
//  - it just isn't done, for whatever reason
// if both versions are 0.*, they have to be the exact same version to be
// considered compatible

// enum for compatibility test outcomes
// IS_COMPATIBLE: full compatibility between versions
// IS_NOT_COMPATIBILITY: no compatibility between versions
// INDETERMINATE_COMPATIBILITY: compatibility is possible but not guaranteed
typedef enum
{
  IS_COMPATIBLE,
  IS_NOT_COMPATIBLE,
  INDETERMINATE_COMPATIBILITY
} LIBLIMEADE_COMPATIBILITY;

// used internally to determine when two versions of the protocol can
// communicate successfully
// local_ver: this program's version
// target_ver: the presumed communicating program's version of this same
// software
LIBLIMEADE_COMPATIBILITY
liblimeade_check_versioning(LIBLIMEADE_VERSION local_ver,
                            LIBLIMEADE_VERSION target_ver);

#endif /* _LIBLIMEADE_ENTRY_H */