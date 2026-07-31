// liblimeade.h
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

#ifndef _POSIX_C_SOURCE
#define _POSIC_C_SOURCE
#endif /* _POSIC_C_SOURCE */

#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>

/*** *** VERSION *** ***/
// liblimeade v0.1
#define LIBLIMEADE_VERSION_STR "0.1"
#define LIBLIMEADE_MAJOR_VERSION 0
#define LIBLIMEADE_MINOR_VERSION 1
extern uint8_t LIBLIMEADE_VERSION[2];



/*** *** CONTEXT *** ***/


// the `self` of the library
typedef struct
{
  uint8_t role;       // either host or client
  uint64_t sessionid; // Limeade session identifier

  // SSH child process to monitor (for client)
  pid_t ssh_child_pid;

  // fds for send/recv
  int send_fd;
  int recv_fd;
} LIMEADE_CONTEXT;

// options for LIMEADE_CONTEXT.role
enum LIMEADE_ROLE
{
  LIMEADE_ROLE_HOST,
  LIMEADE_ROLE_CLIENT
};



/*** *** ERROR BUFFER *** ***/


// since errors can't always be indicated via return values (because not all
// errors are completely fatal, so the actual return values tend to still be
// useful) they are instead indicated via an internal ring buffer that stores
// the last 5 errors (or successes)
#define LIMEADE_ERROR_BUFFER_SIZE 10
// all failable functions in this library utilize this buffer. If a function in
// this library calls another function in the library and that one fails, it
// pushes an error, then the externally called function returns and passes the
// original error on the the user

// all types of errors
typedef enum
{
  // the error that wasn't
  LIMEADE_SUCCESS,

  // errors caused by malformed packet segments
  LIMEADE_ERROR_PACKET_MAGIC,          // improper/no magic
  LIMEADE_ERROR_PACKET_FORMAT,         // improper/no flags
  LIMEADE_ERROR_PACKET_SIZE,           // disingenuous/incorrent packet size
  LIMEADE_ERROR_PACKET_SESSION,        // wrong/no session
  LIMEADE_ERROR_PACKET_DATA,           // malformed/lacking data
  LIMEADE_ERROR_PACKET_COMPRESSED,     // improper compressed data

  // decoding errors
  LIMEADE_ERROR_DECODING_FATAL,    // i.e. packet ends prematurely
  LIMEADE_ERROR_DECODING_NONFATAL, // i.e. unparsable int

  // miscellaneous
  LIMEADE_ERROR_GARBAGE,         // user gave garbage data
  LIMEADE_ERROR_SSH_PROC,        // error spawning SSH process to tunnel
  LIMEADE_ERROR_INVALID_CONTEXT, // LIMEADE_CONTEXT passed was malformed
  LIMEADE_ERROR_MEMORY,          // memory allocation error
  LIMEADE_ERROR_OTHER            // something impossible enough to not define
} LIMEADE_ERROR_TYPE;

typedef uint8_t LIMEADE_ERROR;

// the buffer itself
LIMEADE_ERROR LIMEADE_ERRORS[LIMEADE_ERROR_BUFFER_SIZE];

// manages which index in the buffer is the latest error
// if =1, the latest error is at [1], the previous [0], and the next previous
// [5], etc.
LIMEADE_ERROR LIMEADE_ERROR_INDEX;

/* limeade_inserror
 *
 * inserts an error into the ring buffer
 *
 * no return, can't fail
 */
void limeade_inserror(LIMEADE_ERROR error);

/* limeade_poperror
 *
 * pops the last error from the ring buffer
 *
 * returns the error, can't fail
 */
LIMEADE_ERROR limeade_poperror(void);



/*** *** INITIALIZATION *** ***/


// the name of the SSH subsystem this library represents
#define LIMEADE_SUBSYSTEM_NAME "limeade"

/* limeade_client_init
 *
 * spanws an SSH child process to initialize the subsystem on the SSH host and
 * dups the fds required to send/recv on the tunnel, and stores the necessary data
 * in the return struct
 *
 * port: the network port to attempt the connection on
 * dest: the destination of the connection (user@ipordomain.com)
 *
 * returns a LIMEADE_CONTEXT instance usable for all send/recv/deconstructor
 * functions
 */
LIMEADE_CONTEXT limeade_client_init(uint16_t port, const char *dest);

/* limeade_host_init
 *
 * populates the return struct with the necessary data for communicating through
 * an SSH tunnel, assuming that the program was executed by sshd to act as the
 * subsystem
 *
 * no arguments
 *
 * returns a LIMEADE_CONTEXT instance usable for all send/recv/deconstructor
 * functions
 */
LIMEADE_CONTEXT limeade_host_init();



/*** *** DECONSTRUCTION FUNCTIONS *** ***/


/* limeade_client_free
 *
 * deconstructs and free the connection instance represented by the argument for
 * the client side of the connection
 *
 * self: the context representing the connection to deconstruct
 *
 * no return, can't fail
 */
void limeade_client_free(LIMEADE_CONTEXT self);

/* limeade_host_free
 *
 * deconstructs and frees the connection instance represented by the argument for
 * the host side of the connection
 *
 * self: the context representing the connection to deconstruct
 *
 * no return, can't fail
 */
void limeade_host_free(LIMEADE_CONTEXT self);



/*** *** PACKET ANATOMY *** ***/


// LIMEADE_PACKET
// types of packets in the Limeade suite
enum LIMEADE_PACKET
{
  // init
  LIMEADE_PACKET_ASK,    // initial connection request by client
  LIMEADE_PACKET_ANSWER, // acknowledgement by host

  // data
  LIMEADE_PACKET_EVENT,        // syscall (event) information
  LIMEADE_PACKET_EVENTS,       // more sycalls
  LIMEADE_PACKET_PROC_GENERIC, // proc table information
  LIMEADE_PACKET_PROC_UPDATE,  // dynamic proc table information
  LIMEADE_PACKET_PERF,         // resource usage information

  // other
  LIMEADE_PACKET_COMMANDEER, // request command execution (host only)
  LIMEADE_PACKET_CLOSE       // formally close connection
};

// packet magic at the start and end of every packet
static char LIMEADE_MAGIC[12];
#define LIMEADE_MAGIC_RAW "\xFF\xFFLMPRT^@^\xFF\xFF";

// packet data, while variadic, is consistently visualized into rows & columns.
// As much, there are specific characters used for delimeting fields and row
// breaks
#define LIMEADE_FIELD_DELIM "\xFF" // put between each field
#define LIMEADE_ROW_DELIM   "\xFE" // put between each row

// LIMEADE_ASK
typedef struct
{
  char *hostname;  // client's hostname
  char *kernelver; // client's kernel version
  char *distro;    // client's Linux distribution
  char *ipaddr;    // client's IP address
  char *macaddr;   // client's MAC address
  char *processor; // client's processor's name
  char *processor_vend; // client's processor's vendor
  char *ram;       // amount of RAM on the system
  unsigned int response_wait_secs; // amount of time the client's willing to
  // wait for LIMEADE_ANSWER
} LIMEADE_ASK;

// LIMEADE_ANSWER
typedef struct
{
  uint64_t sessionid; // the client's new assigned sessionID
} LIMEADE_ANSWER;

// LIMEADE_EVENT
typedef struct
{
  uint32_t ts_s;
  uint16_t ts_ms;
  pid_t pid;
  char *syscall;
  char *arg1;
  char *arg2;
  int retval;
} LIMEADE_EVENT;

// LIMEADE_EVENTS
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
  uint16_t total_altered; // no. processes with changed data (including perf)
  uint16_t total_died;    // no. processes that died
  pid_t *died;            // list of dead processes
  LIMEADE_PROC **altered; // list of altered processes with revised data
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
typedef struct {} LIMEADE_CLOSE;



/*** *** SENDING & RECEIVING *** ***/


/* limeade_send
 *
 * sends a full constructed packet
 *
 * this: the context representing the connection
 * type: the type of packet (from enum LIMEADE_PACKET)
 * ... (1): the packet itself (of the struct type equivalent to `type`)
 *
 * 0 on success, -1 on failure
 */
int limeade_send(LIMEADE_CONTEXT self, int type, ...);

// LIMEADE_RECV
// received data before parsing
typedef struct
{
  unsigned int type; // LIMEADE_PACKET value
  size_t len;
  void *data;        // pointer to data
} LIMEADE_RECV;

/* limeade_wait_recv
 *
 * hangs until it receives a full packet from the wire, and returns it
 *
 * returns a LIMEADE_RECV of the packet
 */
LIMEADE_RECV limeade_wait_recv(LIMEADE_CONTEXT self);

// I'm not giving each of these their own docstring
// each of these take a LIMEADE_RECV and the connection context and return a
// specific type of packet. On the library user's side, it'd look something
// like the following:
//
// LIMEADE_RECV tmp = limeade_wait_recv(ctx);
//
// switch(tmp)
// {
//   case LIMEADE_PACKET_ASK:
//     LIMEADE_ASK data = limeade_process_ask(ctx, tmp);
//     -- SNIP --
//   case LIMEADE_PACKET_ANSWER:
//     LIMEADE_ANSWER data = limeade_process_answer(ctx, tmp);
//   -- SNIP --
// }
//
// NOTE ABOUT THESE FUNCTIONS
// they free the `data` argument they're provided from the heap, and their returns live
// on the stack, but any/all pointers in the return value point to the heap
LIMEADE_ASK          limeade_process_ask         (LIMEADE_CONTEXT self, LIMEADE_RECV data);
LIMEADE_ANSWER       limeade_process_answer      (LIMEADE_CONTEXT self, LIMEADE_RECV data);
LIMEADE_EVENT        limeade_process_event       (LIMEADE_CONTEXT self, LIMEADE_RECV data);
LIMEADE_EVENTS       limeade_process_events      (LIMEADE_CONTEXT self, LIMEADE_RECV data);
LIMEADE_PROC_GENERIC limeade_process_proc_generic(LIMEADE_CONTEXT self, LIMEADE_RECV data);
LIMEADE_PROC_UPDATE  limeade_process_proc_update (LIMEADE_CONTEXT self, LIMEADE_RECV data);
LIMEADE_PERF         limeade_process_perf        (LIMEADE_CONTEXT self, LIMEADE_RECV data);
LIMEADE_COMMANDEER   limeade_process_commandeer  (LIMEADE_CONTEXT self, LIMEADE_RECV data);
// no reason for limeade_process_close

// it is because of such heapiness that this exists:
/* limeade_release
 *
 * frees limeade packet data from heap
 *
 * type: describes the type of the data you want to free, either
 *       LIMEADE_PACKET_TYPE or -1 for LIMEADE_RECV
 *
 * always succeeds, no return value
 */
void limeade_release(int type, ...);



/*** DIAGNOSTICS ***/


// the following functions are for diagnostic purposes. While they can be
// included in production code, debugging should be disabled in production code
// and enabled during test, both of which are acheived by the following functions:
void limeade_enable_debugging();
void limeade_disable_debugging();

// setting verbosity
// setting the verbosity outside 1-3 does nothing to the value
void limeade_set_verbosity(int level);

// the following functions print visual representation of data types endemic to
// this library
void limeade_diag_repr_context(LIMEADE_CONTEXT ctx);
void limeade_diag_repr_recv(LIMEADE_RECV recv);
void limeade_diag_repr_pkt(int type, ...);

// diagnostically prints all data associated with the error buffer
void limeade_diag_repr_errors(void);

#endif /* _LIBLIMEADE_H_ */
