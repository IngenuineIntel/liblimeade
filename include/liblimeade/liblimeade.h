// liblimeade.h
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

#ifndef _LIBLIMEADE_H_
#define _LIBLIMEADE_H_

#include <netinet/in.h>
#include <semaphore.h>
#include <stdint.h>
#include <sys/socket.h>

#if __has_include(<libssh2.h>)
#  include <libssh2.h>
#  define LIMEADE_HAS_LIBSSH2 1
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

/*** *** ERROR MANAGEMENT *** ***/
// the various errors liblimeade functions can return
enum limeade_error
{
  LIMEADE_SUCCESS = 0,
  LIMEADE_OK      = 0,

  // errors while sending
  LIMEADE_ERROR_GARBAGE, // user supplied useless data to the library
  LIMEADE_ERROR_COMPRESSION, // errors when compressing

  // errors with particular libraries/intefaces
  LIMEADE_ERROR_LIBSSH,    // error with libssh
  LIMEADE_ERROR_SSH_CHILD, // error with SSH child process
  LIMEADE_ERROR_MONOTONIC, // error with monotonic timestamps
  LIMEADE_ERROR_NETWORK,   // error with networking
  LIMEADE_ERROR_CSM,       // CSM either failed to start or died

  // errors with decoding
  LIMEADE_ERROR_NO_DATA,         // no data on port
  LIMEADE_ERROR_BAD_MAGIC,       // improper/no magic
  LIMEADE_ERROR_BAD_FORMAT,      // indecipherable flags
  LIMEADE_ERROR_BAD_DATA,        // faliure to parse data
  LIMEADE_ERROR_BAD_COMPRESSION, // failure to decompress


  // miscellaneous
  LIMEADE_ERROR_REJECTED,        // no acknowledgement from ricipient
  LIMEADE_ERROR_INVALID_CONTEXT, // bad limeade_context* passed
  LIMEADE_ERROR_MEMORY,          // failure to allocate memory
  LIMEADE_ERROR_NOT_SUPPORTED,   // feature not supported
  LIMEADE_ERROR_TH_RECV_DIED,    // receiving thread died
  LIMEADE_ERROR_TH_CSM_DIED,     // CSM thread died
  LIMEADE_ERROR_OTHER,           // unspecified & probably assumed impossible
  
  LIMEADE_MAXIMUM_ERROR
};

// textual equivalents of errors
static const char *LIMEADE_ERROR_REPRS[] = {
  "Success/No error",

  "Garbage data supplied",
  "Compression failure",

  "libssh failure",
  "SSH failure",
  "Monotonic benchmark failure",
  "Network failure",
  "CSM failure",

  "No data on port",
  "Bad magic",
  "Bad format",
  "Bad data",
  "Bad compression",
  
  "No acknowledgement from recipient",
  "Invalid context",
  "Allocation failure",
  "Feature not supported",
  "Receiving thread died",
  "Compression management thread died",
  "Other",
  "Unknown"
};

/*** *** INIT *** ***/
// this library can operate in various different modes, one of which must be
// selected
enum limeade_mode
{
  // When a subsystem is configured in sshd and that subsystem is requested by
  // the client, sshd spawns a configured executable (i.e. /usr/bin/sftp) to
  // manage the connection. Sshd pipes the program's stdin/out to itself, and
  // tunnels the information to and from the client and program through itself,
  // managing the encryption and network connection on its own.
  // This mode is designed to be compatible with this system, as if it were
  // spawned by sshd.
  LIMEADE_MODE_HOST_SSH = 0,

  // There is a variation of this for the client, too; this mode spawns an ssh
  // process and pipes its stdin/out to itself, and uses the child process to
  // manage the networking/encryption.
  LIMEADE_MODE_CLIENT_SSH = 1,

  // The client can also do networking/encryption in-house with libssh.
  LIMEADE_MODE_CLIENT_LIBSSH,

  // this protocol is not constrained to SSH, though. It can also exist as a
  // bare-bones ethernet protocol, using simple UDP to share data. This method
  // sacrifices security for speed and simplicity
  LIMEADE_MODE_HOST_ETH,   // UDP server
  LIMEADE_MODE_CLIENT_ETH, // UDP client
};

// packets are sent to the standard port of the Limeade protocol:
#define LIMEADE_PORT 12046

// the name of the SSH subsystem Limeade uses
#define LIMEADE_SUBSYSTEM "limeade"

// additional to these modes, there are compression settings to choose from
enum limeade_compression_mode
{
  // there are 3 options for compression

  // 1. no compression
  LIMEADE_MODE_NO_COMPRESSION = 16,

  // 2. low compression all the time
  LIMEADE_MODE_LOW_COMPRESSION = 32,

  // 3. medium compression all the time
  LIMEADE_MODE_MED_COMPRESSION = 48,

  // 4. high compression all the time
  LIMEADE_MODE_HIGH_COMPRESSION = 64,
};

#define LIMEADE_NR_PKTS 8 // number of packets stored at a time
#define LIMEADE_MAX_PKT_SZ 65536 // maximum size of packet

// TODO really good documentation
struct limeade_indiv_recv
{
  pthread_mutex_t mtx;

  char data[LIMEADE_MAX_PKT_SZ];
  uint32_t sz;
  
  // only populated when running as UDP host
  struct sockaddr_in addr;
  socklen_t addr_len;

  union
  {
    uint8_t flags;

    struct
    {
      uint8_t been_read:1; // if the packet has been read
      uint8_t ready:1;     // if the packet is ready to be read
    };
  };
};

#define LIMEADE_TH_RECV_DFLT_HZ 4
// TODO really good documentation
struct limeade_recv_data
{
  // readonly
  pthread_t tid;
  uint16_t hz; // frequency to operate at (kindof)

  pthread_mutex_t mtx_unread;
  uint16_t nr_pkts_unread; // total number of packets that haven't been read

  pthread_mutex_t mtx_nr_read;
  uint32_t nr_pkts_read; // total number of packet that have been read

  pthread_mutex_t mtx_lost;
  uint32_t nr_pkts_lost; // number of packets that were overwritten without
                         // being read (number of packets lost)

  pthread_mutex_t mtx_idx;
  uint16_t idx_read, idx_write; // indexes for the next packet to read, & the next
                                // packet to write

  struct limeade_indiv_recv pkts[LIMEADE_NR_PKTS]; // the packets themselves

  struct limeade_indiv_recv ack; // acknowledgement packets go here

  sem_t sem_kys; // kill switch for recv thread
};

// TODO really good documentation
struct limeade_context
{
  // readonly
  uint8_t mode;
  uint8_t compr_mode;

  pthread_mutex_t mtx_compr_lvl;
  uint8_t compr_lvl;

  pthread_mutex_t mtx_rfd; // (for using the file descriptor, not writing the memory)
  int rfd; // fd for receiving

  pthread_mutex_t mtx_sfd;
  int sfd; // fd for sending

  pthread_mutex_t mtx_pub_attrs;
  char *dest;
  int port;
  uint64_t sessionid;
  uint32_t ack_wait_time_ms;

  pthread_mutex_t mtx_mode_specific;
  union
  {
    pid_t ssh_pid; // LIMEADE_MODE_CLIENT_SSH

    struct sockaddr_in saddr; // LIMEADE_MODE_CLIENT_ETH
    socklen_t saddr_len;

    // must be populated before calls to send packets
    struct sockaddr_in cliaddr; // LIMEADE_MODE_HOST_ETH
    socklen_t cliaddr_len;
  };

  struct limeade_recv_data recv;

};

/* limeade_init
 *
 * creates a context that can be passed to other functions to send/recv data
 * through the Limeade protocol
 *
 * ctx: the context object to populate
 * flags: a combination of `limeade_mode` and `limeade_compression_mode`
 *        for example, `LIMEADE_MODE_CLIENT_LIBSSH|LIMEADE_MODE_LOW_COMPRESSION`
 *        would be a valid flags argument
 * ...:
 *   if LIMEADE_MODE_CLIENT_*SSH:
 *     const char *dest // like "user@machine"
 *   if LIMEADE_MODE_CLIENT_ETH:
 *     const char *dest, // host IP address
 *     int port          // port (generally LIMEADE_PORT)
 *   if LIMEADE_MODE_HOST_ETH:
 *     int port          // port (generally LIMEADE_PORT)
 *
 * on success, returns a valid LIMEADE_CONTEXT*
 * on fail,    returns NULL and pushes an error to the error buffer
 */
int limeade_init(struct limeade_context *ctx, int flags, ...);
// after limeade_init, to make the initial network connection:
int limeade_connect(struct limeade_context *ctx);
// this is done in a separate in case the user wants to hack the library for
// initializing the connection.

// to deconstruct a struct limeade_context:
void limeade_destruct(struct limeade_context *ctx);



/*** *** PACKET STRUCTURE *** ***/

// for each kind of packet, there is a numeric representation here
enum limeade_packet
{
  LIMEADE_PACKET_KNOCK,        // connection initialization (equivalent of SYN)
  LIMEADE_PACKET_RECOGNIZE,    // confirms connection
  LIMEADE_PACKET_INTRODUCTION, // data about the client machine
  LIMEADE_PACKET_ACKNOWLEDGE,  // acknowledgement of packet received
  LIMEADE_PACKET_EVENTS,       // event data
  LIMEADE_PACKET_PROC_GENERIC, // process table data
  LIMEADE_PACKET_PROC_UPDATE,  // process table data (dynamic)
  LIMEADE_PACKET_PERF,         // resource usage/performance data
  LIMEADE_PACKET_COMMANDEER,   // request command execution on the client
  LIMEADE_PACKET_EXITED,       // indication of the command's completion
  LIMEADE_PACKET_CLOSE,        // connection deinitialization

  LIMEADE_PACKET_MAX
};

// aliases for conveience
#define LIMEADE_PACKET_ACK   LIMEADE_PACKET_ACKNOWLEDGE
#define LIMEADE_PACKET_INTRO LIMEADE_PACKET_INTRODUCTION

// limeade_packet_data
// a wrapper for a buffer of data to be sent out
struct limeade_packet_data
{
  uint8_t type;    // type of packet
  uint8_t compr;   // current compression level
  uint32_t pkt_sz; // size of packet in memory
  void *pkt;   // pointer to beginning of packet
  void *flags; // .pkt   + sizeof(LIMEADE_MAGIC)
  void *data;  // .flags + sizeof(struct limeade_packet_flags);
};

// every packet starts with a magic
#define LIMEADE_MAGIC_RAW "\xFF\xFFLIME^0^\xFF\xFF"
static const char LIMEADE_MAGIC[] = LIMEADE_MAGIC_RAW;

// every client is given a session ID with which to distinguish itself
// as a type:
typedef uint64_t LIMEADE_SESSION;

// packet data, while variadic, is consistenly visualized into rows
// & columns, Likewise, there are specific characters used for delimiting
// fields & rows:
#define LIMEADE_FIELD_DELIM '\xFF' // put between each field...
#define LIMEADE_ROW_DELIM   '\xFE' // unless this is there to separate the rows

// limeade_packet_flags
// representation of packet flags in memory
struct limeade_packet_flags
{
  uint16_t packet_size; // total size of packet (excluding magic)
  uint8_t type:4;       // type of packet (limeade_packet)
  uint8_t compr_lvl:4;  // level of compression (0 for none)
  uint32_t ts_s;        // UNIX time at send time
  uint16_t ts_ms;       // milliseconds since UNIX time at send time
  union
  {
    uint8_t reserved[7];
  };
  LIMEADE_SESSION session;
};

// equivalent to LIMEADE_PACKET_KNOCK
struct limeade_knock
{
  uint8_t prev_connected;       // if this client has connected before
  LIMEADE_SESSION prev_session; // session used previously (can be 0)
};

// equivalent to LIMEADE_PACKET_RECOGNIZE
struct limeade_recognize
{
  uint8_t accepted; // if the client is accepted (generally not no)
  LIMEADE_SESSION new_session; // new session the server assigns
};

// equivalent to LIMEADE_PACKET_INTRODUCTION
struct limeade_introduction
{
  char *hostname;    // hostname of the client machine
  char *kernelver;   // Linux version on the client machine
  char *distro;      // distribution of the client machine
  char *origin_user; // user who initialized the connection
  char *processor;   // processor of the client machine
  char *vendor;      // vendorID of the processor
  uint32_t ram_mbs;  // total RAM
  uint32_t swap_mbs; // total virtual memory
};
#define limeade_intro limeade_introduction

// equivalent to LIMEADE_PACKET_ACKNOWLEDGE
struct limeade_acknowledge
{
  uint32_t send_ts_s;
  uint16_t send_ts_ms;
  uint32_t recv_ts_s;
  uint32_t recv_ts_ms;
};
#define limeade_ack limeade_acknowledge

// each event individually
struct limeade_indiv_event
{
  uint32_t ts_s;  // when the event was recorded
  uint16_t ts_ms; //
  pid_t pid;      // PID of the calling process
  char *syscall;  // type of syscall (represented as a string for consistency)
  char *arg1;     // value in/referenced in RDI
  char *arg2;     // value in/referenced in RSI
  int retval;     // return value
};

// equivalent to LIMEADE_PACKET_EVENTS
struct limeade_events
{
  uint16_t nr_events;
  struct limeade_indiv_event *events;
};

// each process individually
struct limeade_indiv_proc
{
  pid_t pid;
  pid_t ppid;
  uid_t uid;
  uint16_t threads;
  union             // representations of CPU usage
  {
    uint32_t cpu_ticks;
  };
  union             // representations of memory usage
  {
    uint32_t vm_rss_kb;
    uint32_t mem_kb;
    uint32_t ram_kb;
  };
  char *command;
};

// equivalent to LIMEADE_PACKET_PROC_GENERIC
struct limeade_proc_generic
{
  uint16_t total;
  struct limeade_indiv_proc *procs;
};

// equivalent to LIMEADE_PACKET_PROC_UPDATe
struct limeade_proc_update
{
  uint16_t total_died;                 // total dead processes
  uint16_t total_altered;              // total altered processes
  pid_t *died;                         // list of dead processes
  struct limeade_indiv_proc *altered; // data of altered processes
};

// equivalent to LIMEADE_PACKET_PERF
struct limeade_perf
{
  uint8_t cores;
  uint32_t avg_cpu_pct;
  uint32_t mem_total_kb;
  uint32_t mem_free_kb;
  uint32_t mem_available_kb;
  uint32_t mem_cached_kb;
  float load_1m;
  float load_5m;
  float load_15m;
  char *other;
};

// equivalent to LIMEADE_PACKET_COMMANDEER
struct limeade_commandeer
{
  char *command;  // command to execute
  uint16_t flags; // (see below)
  uint32_t id;    // ID to reference with
};

// flags options for struct limeade_commandeer
enum limeade_commandeer_flags
{
  LIMEADE_COMMANDEER_WO_JAIL   = 1, // otherwise, executed in jail
  LIMEADE_COMMANDEER_HIGH_PRIV = 2, // otherwise, executed with normal privledges
  LIMEADE_COMMANDEER_TTY       = 4, // otherwise, executed without TTY environment
};
// for example, LIMEADE_COMMANDEER_WO_JAIL|LIMEADE_COMMANDEER_HIGH_PRIV would
// be a value `flags` value

// equivalent to LIMEADE_PACKET_EXITED
struct limeade_exited
{
  uint32_t id;  // ID supplied in COMMANDEER
  int exitcode; // exit code of the process
};

// equivalent to LIMEADE_PACKET_CLOSE
struct limeade_close
{
  char *explanation;
};



/*** *** SENDING AND RECEIVING *** ***/


/* limeade_send
 *
 * sends a full constructed packet
 *
 * ...: struct limeade_* associated with `type`
 *
 * 0 on success, -1 on error, and the error is pushed to the error buffer
 */
int limeade_send(struct limeade_context *ctx, enum limeade_packet type, ...);

/* limeade_send_await
 *
 * sends a full constructed packet and waits for acknowledgement from the peer
 *
 * ...: struct limeade_* associated with `type`
 *
 * 0 on success, -1 on error, and the error is push to the error buffer
 */
int limeade_send_await(struct limeade_context *ctx, enum limeade_packet type, ...);

// struct limeade_recv
// received data before parsing
#define limeade_recvd limeade_packet_data

// limeade_recv: receives a packet & sends an acknowledgement, but all the data will be NULL if there are no packets
// limeade_recv_wait: receives a packet & sends an acknowledgement, but waits if there are no packets to receive
// limeade_recv_noreply: receives a packet without acknowledgement, but all data will be NULL if there are no packets
// limeade_recv_wait_noreply: receives a packet without acknowledgement, but waits for a packet to arrive
int limeade_recv(struct limeade_context *ctx, struct limeade_recvd *out);
int limeade_recv_wait(struct limeade_context *ctx, struct limeade_recvd *out, int wait_ms);
int limeade_recv_noreply(struct limeade_context *ctx, struct limeade_recvd *out);
int limeade_recv_wait_noreply(struct limeade_context *ctx, struct limeade_recvd *out, int wait_ms);

// note: wait_ms <= 0 will wait forever

// flags can be extracted as so:
struct limeade_packet_flags limeade_parse_flags(struct limeade_recvd data);

// for the packet's body, there is a function for each packet type, and the
// type should be checked before parsing:
//
//struct limeade_recv pkt = limeade_recv(ctx);
//
//switch(pkt.type)
//{
//  case LIMEADE_PACKET_KNOCK:
//    struct limeade_knock data;
//    if(limeade_parse_knock(&data, pkt) != LIMEADE_OK)
//    {
//      --SNIP--
//    }
//  --SNIP--
//  case LIMEADE_PACKET_RECOGNIZE:
//    struct limeade_recognize data;
//    if(limeade_parse_recognize(&data, pkt) != LIMEADE_OK)
//    {
//      --SNIP--
//    }
//  --SNIP--
//}
//
// note that if the wrong function is called for parsing, it will detect it
// and return a LIMEADE_ERROR_GARBAGE
int limeade_parse_knock(struct limeade_knock *out, struct limeade_recvd *pkt);
int limeade_parse_recognize(struct limeade_recognize *out, struct limeade_recvd *pkt);
int limeade_parse_intro(struct limeade_intro *out, struct limeade_recvd *pkt);
#define limeade_parse_introduction limeade_parse_intro
int limeade_parse_ack(struct limeade_ack *out, struct limeade_recvd *pkt);
#define limeade_parse_acknowledge limeade_parse_ack
int limeade_parse_events(struct limeade_events *out, struct limeade_recvd *pkt);
int limeade_parse_proc_generic(struct limeade_proc_generic *out, struct limeade_recvd *pkt);
int limeade_parse_proc_update(struct limeade_proc_update *out, struct limeade_recvd *pkt);
int limeade_parse_perf(struct limeade_perf *out, struct limeade_recvd *pkt);
int limeade_parse_commandeer(struct limeade_commandeer *out, struct limeade_recvd *pkt);
int limeade_parse_exited(struct limeade_exited *out, struct limeade_recvd *pkt);
int limeade_parse_close(struct limeade_close *out, struct limeade_recvd *pkt);

// the following functions are for releasing the parsed data created by the
// above functions. They are not meant to be called directly: see below
void limeade_release_knock(struct limeade_knock *in);
void limeade_release_recognize(struct limeade_recognize *in);
void limeade_release_intro(struct limeade_intro *in);
#define limeade_release_introduction limeade_release_intro
void limeade_release_ack(struct limeade_ack *in);
#define limeade_release_acknowledge limeade_release_ack
void limeade_release_events(struct limeade_events *in);
void limeade_release_proc_generic(struct limeade_proc_generic *in);
void limeade_release_proc_update(struct limeade_proc_update *in);
void limeade_release_perf(struct limeade_perf *in);
void limeade_release_commandeer(struct limeade_commandeer *in);
void limeade_release_exited(struct limeade_exited *in);
void limeade_release_close(struct limeade_close *in);

void limeade_release_recvd(struct limeade_recvd *in);

// for convenience:
#define limeade_release(in) _Generic((in), \
  struct limeade_knock*:        limeade_release_knock,\
  struct limeade_recognize*:    limeade_release_recognize,\
  struct limeade_intro*:        limeade_release_intro,\
  struct limeade_ack*:          limeade_release_ack,\
  struct limeade_events*:       limeade_release_events,\
  struct limeade_proc_generic*: limeade_release_proc_generic,\
  struct limeade_proc_update*:  limeade_release_proc_update,\
  struct limeade_proc_perf*:    limeade_release_perf,\
  struct limeade_commandeer*:   limeade_release_commandeer,\
  struct limeade_exited*:       limeade_release_exited,\
  struct limeade_close*:        limeade_release_close,\
  struct limeade_recvd*:        limeade_release_recvd,\
  struct limeade_context*:      limeade_destruct\
)(in)


/*** DIAGNOSTICS ***/
// perror-like error system
void limeade_perror(const char * const s, const int error);

#endif /* _LIBLIMEADE_H_ */
