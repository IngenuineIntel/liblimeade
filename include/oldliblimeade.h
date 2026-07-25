// liblimeade.h
// implements the Limeade protocol
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

#ifndef _LIBLIMEADE_H_
#define _LIBLIMEADE_H_

#include <stdint.h>

/*** ENUMS ***/

// LIMEADE_ROLE
// a liblimeade instance's role in a connection
// either client or host, pretty straight forward
enum LIMEADE_ROLE
{
  LIMEADE_ROLE_HOST   = 1,
  LIMEADE_ROLE_CLIENT = 2
};

// LIMEADE_PACKET
// types of packets in the Limeade suite
enum LIMEADE_PACKET
{
  // init
  LIMEADE_PACKET_ASK,    // initial connection request by client
  LIMEADE_PACKET_ASNWER, // acknowledgement by host

  // data
  LIMEADE_PACKET_EVENT,        // syscall (event) information
  LIMEADE_PACKET_EVENTS,       // more sycalls
  LIMEADE_PACKET_PROC_GENERIC, // proc table information
  LIMEADE_PACKET_PROC_UPDATE,  // dynamic proc table information
  LIMEADE_PACKET_PERF,         // resource usage information

  // other
  LIMEADE_PACKET_COMMANDEER, // request command execution
  LIMEADE_PACKET_CLOSE       // formally close connection
};

/*** DATA TYPES ***/

// liblimeade v0.1
#define LIBLIMEADE_MAJOR_VERSION 0
#define LIBLIMEADE_MINOR_VERSION 1
static uint8_t LIBLIMEADE_VERSION[2];

// packet magic
#define LIMEADE_MAGIC "\xFF\xFFLMPRT^@^\xFF\xFF";

// packet field & row delimeter
#define LIMEADE_ROW_DELIM "\xFE"
#define LIMEADE_FIELD_DELIM "\xFF"

// LIMEADE_CONTEXT
// the "self" or "this" of the library
typedef struct
{
  enum LIMEADE_ROLE role;
  uint64_t sessionid; // Limeade session identifier

  // SSH child process to monitor (for client)
  pid_t ssh_child_pid;

  // fds for send/recv
  int send_fd;
  int recv_fd;
} LIMEADE_CONTEXT;



// LIMEADE_SUBSYSTEM_NAME
#define LIMEADE_SUBSYSTEM_NAME "limeade"

// LIMEADE_ASK
// data in a LIMEADE_ASK packet
typedef struct
{
  char *hostname;  // client's hostname
  char *kernelver; // client's kernel version
  char *distro;    // client's Linux distribution
  char *ipaddr     // client's IP address
  char *macaddr    // client's MAC address
  char *processor; // client's processor's name
  char *processor_vend; // client's processor's vendor
  char *ram;       // amount of RAM on the system
  unsigned int response_wait_secs; // amount of time the client's willing to
  // wait for LIMEADE_ANSWER
} LIMEADE_ASK;

// LIMEADE_ANSWER
// data in LIMEADE_ANSWER packet
typedef struct
{
  uint64_t sessionid; // the client's new assigned sessionID
} LIMEADE_ANSWER;

// LIMEADE_EVENT
// data in LIMEADE_EVENT packet
typedef struct
{
  uint32_t ts_s;
  uint16_t ts_ms
  pid_t pid;
  char *syscall;
  char *arg1;
  char *arg2;
  int retval;
} LIMEADE_EVENT;

typedef struct
{
  uint16_t nr_events;
  LIMEADE_EVENT **events;
} LIMEADE_EVENTS;

// LIMEADE_PROC
// single process in process table
typedef struct
{
  pid_t pid;
  pid_t ppid;
  uid_t uid;
  uint16_t threads;   // no. threads
  uint32_t cpu_ticks; // CPU usage
  uint32_t vm_rss_kb; // memory usage (in KB)
  char *command;
} LIMEADE_PROC;

// LIMEADE_PROC_GENERIC
typedef struct
{
  uint32_t ts_s;
  uint16_t ts_ms;
  uint16_t total;       // no. of processes
  LIMEADE_PROC **procs; // process data
} LIMEADE_PROC_GENERIC;

// LIMEADE_PROC_UPDATE
typedef struct
{
  uint32_t ts_s;
  uint16_t ts_ms;
  uint16_t total_altered;  // no. processes with changed data (including perf)
  uint16_t total_died;     // no. processes that died
  pid_t *died[];           // list of dead processes
  LIMEADE_PROC *altered[]; // list of altered processes with revised data
} LIMEADE_PROC_UPDATE;

// LIMEADE_PERF
typedef struct
{
  uint32_t ts_s;
  uint16_t ts_ms;
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
} LIMEADE_PERF;

// LIMEADE_COMMANDEER
typedef struct
{
  char *command;
  uint8_t exec_with_root;
  uint8_t exec_with_tty;
  uint8_t exec_with_jail;
} LIMEADE_COMMANDEER;

// LIMEADE_CLOSE
typedef struct LIMEADE_CLOSE;

// LIMEADE_RECV
// received data before parsing
typedef struct
{
  unsigned int type; // LIMEADE_PACKET value
  void *data;        // pointer to data
} LIMEADE_RECV;

/*** FUNCTIONS ***/

LIMEADE_CONTEXT limeade_client_init(int port, const char *user, const char *host);
LIMEADE_CONTEXT limeade_host_init();

int limeade_send(LIMEADE_CONTEXT this, int type, ...);

LIMEADE_RECV limeade_wait_recv(LIMEADE_CONTEXT this);
LIMEADE_ASK limeade_process_ask(LIMEADE_CONTEXT this, LIMEADE_RECV data);
LIMEADE_ANSWER limeade_process_answer(LIMEADE_CONTEXT this, LIMEADE_RECV data);
LIMEADE_EVENT limeade_process_event(LIMEADE_CONTEXT this, LIMEADE_RECV data);
LIMEADE_PROC_GENERIC limeade_process_proc_generic(LIMEADE_CONTEXT this, LIMEADE_RECV data);
LIMEADE_PROC_UPDATE limeade_process_proc_update(LIMEADE_CONTEXT this, LIMEADE_RECV data);
LIMEADE_PERF limeade_process_perf(LIMEADE_CONTEXT this, LIMEADE_RECV data);


/*** ERROR MANAGEMENT ***/

// since errors can't always be indicated via return values (because not all
// errors are completely fatal, so the actual return values tend to still be
// useful) they are instead indicated via an internal buffer that stores the
// last 5 errors (orTuccesses)

// not all functions insert into this buffer, but most do
// the origin of the error is generally indicated by its name

// all types of errors
typedef enum
{
  // the error that wasn't
  LIMEADE_SUCCESS,

  // dependency issues
  LIMEADE_ERROR_SSH,
  LIMEADE_ERROR_ZLIB,

  // errors caused by malformed packet segments
  LIMEADE_ERROR_PACKET_MAGIC,      // improper/no magic
  LIMEADE_ERROR_PACKET_FORMAT,     // improper/no flags
  LIMEADE_ERROR_PACKET_SIZE,       // disingenuous/incorrent packet size
  LIMEADE_ERROR_PACKET_SESSION,    // wrong/no session
  LIMEADE_ERROR_PACKET_DATA,       // malformed/lacking data
  LIMEADE_ERROR_PACKET_COMPRESSED, // improper compressed data

  // miscellaneous
  LIMEADE_ERROR_GARBAGE,         // user gave garbage data
  LIMEADE_ERROR_TIMEOUT,         // timeout reached before packet end was observed
  LIMEADE_ERROR_INVALID_CONTEXT, // LIMEADE_CONTEXT passed was malformed
  LIMEADE_ERROR_MEMORY,          // memory allocation error
  LIEMADE_ERROR_OTHER            // something impossible enough to not define
} LIMEADE_ERROR_TYPE;

typedef unsigned int LIMEADE_ERROR;

static uint8_t LIMEADE_ERRORS[5];

// which index in the buffer is the latest error
// if 1, the latest error is at [1], the previous [0], and the next previous
// [5], etc.
static uint8_t LIMEADE_ERROR_INDEX;

// errors are added by a handler function,
void limeade_inserror(LIMEADE_ERROR_TYPE error);

// and read (popped, actually) by another handler function
LIMEADE_ERROR_TYPE limeade_poperror(void);

#endif /* _LIBLIMEADE_H_ */
