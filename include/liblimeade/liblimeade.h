// liblimeade.h
//
// [AGPL stuff]

#ifndef _LIBLIMEADE_H_
#define _LIBLIMEADE_H_

#include <stdint.h>
#include <sys/socket.h>

/*** *** ERROR MANAGEMENT *** ***/
// errors are stored in a ring buffer that can be pushed & popped via wrappres.
#define LIMEADE_ERROR_BUFFER_SIZE 10 // nr of errors in buffer

typedef uint8_t LIMEADE_ERROR;

// the ring buffer is accessed through the following functions, which are thread
// safe & don't fail
// limeade_pusherr: pushes an error to the ring buffer
// limeade_poperr : pops an error from the ring buffer1
void limeade_inserr(LIMEADE_ERROR err);
LIMEADE_ERROR limeade_poperr(void);

enum limeade_error
{
  LIMEADE_BLANK_ERROR = 0, // unset error
  LIMEADE_SUCCESS,

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
  LIMEADE_ERROR_OTHER,           // unspecified & probably assumed impossible
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
  LIMEADE_MODE_HOST_SSH,

  // There is a variation of this for the client, too; this mode spawns an ssh
  // process and pipes its stdin/out to itself, and uses the child process to
  // manage the networking/encryption.
  LIMEADE_MODE_CLIENT_SSH,

  // The client can also do networking/encryption in-house with libssh.
  LIMEADE_MODE_CLIENT_LIBSSH,

  // this protocol is not constrained to SSH, though. It can also exist as a
  // bare-bones ethernet protocol, using simple UDP to share data. This method
  // sacrifices security for speed and simplicity
  LIMEADE_MODE_HOST_ETH,   // UDP server
  LIMEADE_MODE_CLIENT_ETH, // UDP client
};

// note for users of the ethernet protocol:
// packets are sent to the standard port of the Limeade protocol:
#define LIMEADE_PORT 12046

// the name of the SSH subsystem Limeade uses
#define LIMEADE_SUBSYSTEM "limeade"

// you have to change this line and recompile the library to change this

// additional to these modes, there are compression settings to choose from
enum limeade_compression_mode
{
  // there are 3 options for compression

  // 1. no compression
  LIMEADE_MODE_NO_COMPRESSION = 0,

  // 2. low compression all the time
  LIMEADE_MODE_LOW_COMPRESSION = 16,

  // 3. medium compression all the time
  LIMEADE_MODE_MED_COMPRESSION = 32,

  // 4. high compression all the time
  LIMEADE_MODE_HIGH_COMPRESSION = 48,

  // 5. start a thread that monitors compression time and bandwidth and
  // dynamically configures the compression level to maximize all resources,
  // called the Compression Supervisor Module or CSM
  // 5.1. bias the CSM towards saving bandwidth
  LIMEADE_MODE_CSM_SAVE_BANDWIDTH = 64,
  // 5.2. bias the CSM towards saving CPU cycles at the expense of bandwidth
  LIMEADE_MODE_CSM_SAVE_CYCLES = 80,
  // 5.3 bias the CSM to maximize throughput at the cost of CPU and bandwidth
  // indiscriminately
  LIMEADE_MODE_CSM_MAXIMIZE = 96,
};
// note for CSM users:
// CSM cannot be used in junction with LIMEADE_MOST_*_SSH options. This is
// because it's impossible to reliably measure CPU processing time to send a
// packet when another process is doing all the hard work. If such a
// collision occures, any CSM argument will be replaced with
// LIMEADE_MODE_LOW_COMPRESSION.

// limeade_csm_data
// data used when utilizing CSM

// the data for a single entry of compression data
// note: compression level is not passed because it can be inferred
struct limeade_csm_compression_entry
{
  uint32_t compr_lvl;    // compression level
  uint32_t precompr_sz;  // size of data before compression
  uint32_t postcompr_sz; // size of data after compression
  uint32_t elapsed_ms;   // elapsed time in milliseconds
};

// the data for a single entry of latency data
struct limeade_csm_latency_entry
{
  uint32_t send_sz;    // size of data sent
  uint32_t elapsed_ms; // elapsed time in milliseconds
};

struct limeade_csm_data
{
  struct limeade_csm_compression_entry **hist_compr;  // compression benchmarks
  struct limeade_csm_latency_entry     **hist_latent; // latency benchmarks

  uint32_t hist_compr_sz;   // size of compression benchmark ring buffer
  uint32_t hist_latent_sz;  // size of latency benchmark ring buffer
  uint32_t hist_compr_idx;  // current index in compression benchmarks
  uint32_t hist_latent_idx; // current index in latency benchmarks
  float freq_s; // frequency that CSM operates at (iter/sec)
  void *id;     // thread ID (*pthread_t)
};

// wrapper functions for adding data to benchmark ring buffers
void limeade_csm_add_compr_entry(void *ctx, struct limeade_csm_compression_entry *entry);
void limeade_csm_add_latency_entry(void *ctx, struct limeade_csm_latency_entry *entry);

// size of hist_* ring buffers
#define LIMEADE_CSM_BENCH_BUFFER_SIZE 30
// frequency
// can be altered after calling `limeade_init` with:
//
//pthread_mutex_lock(ctx->csm_mtx);
//ctx->csm->freq_s = 3
//pthread_mutex_unlock(ctx->csm_mtx);
//
// likewise, this can be used for any component of ctx->csm
//
#define LIMEAE_CSM_FREQ_S 2 // every 2 seconds

// on a host machine (particularly LIMEADE_MODE_HOST_ETH), a list of clients has to be
// stored

// individual client
struct limeade_eth_host_indiv_client
{
  uint64_t session;
  void *addr; //(struct sockaddr*)
};

// limeade_context
// serves as the context object for the functions in this library
struct limeade_context
{
  union
  {
    uint8_t reserved[6];
  };

  uint8_t mode; // mode of operation
  uint8_t compr_mode; // mode of compression

  uint8_t compr_lvl; // level of compression
  void *compr_lvl_mtx; // mutex for compr_lvl

  union
  {
    pid_t ssh_pid;  // LIMEADE_MODE_CLIENT_SSH
    void *ssh_data; // LIMEADE_MODE_CLIENT_LIBSSH
    struct          // LIMEADE_MODE_HOST_ETH
    {
      uint32_t nr_clients;
      struct limeade_eth_host_indiv_client **clients;
    };
    struct // for LIMEADE_MODE_CLIENT_ETH
    {
      struct sockaddr_in *saddr;
      socklen_t saddr_len;
    };
  };

  int sfd; // file descriptor for sending
  int rfd; // file descriptor for receiving
  void *fd_mtx; // mutex for file descriptors

  // data for CSM
  struct limeade_csm_data *csm;
  void *csm_mtx; // mutex for csm

  // attributes intended to be read/write
  char *destination;  // if client, URI of host
  int port;           // port to connect on
  uint64_t sessionid; // sessionID (might be changed internally)
};

/* limeade_init
 *
 * creates a context that can be passed to other functions to send/recv data
 * through the Limeade protocol
 *
 * flags: a combination of `limeade_mode` and `limeade_compression_mode`
 *        for example, `LIMEADE_MODE_CLIENT_LIBSSH|LIMEADE_MODE_LOW_COMPRESSION`
 *        would be a valid flags argument
 * ...:
 *   if LIMEADE_MODE_CLIENT_*SSH:
 *     const char *dest // like "user@machine"
 *   if LIMEADE_MODE_CLIENT_ETH:
 *     const char *dest, // host IP address
 *     int port          // port (generally LIMEADE_PORT)
 *
 * on success, returns a valid LIMEADE_CONTEXT*
 * on fail,    returns NULL and pushes an error to the error buffer
 */
struct limeade_context *limeade_init(uint8_t flags, ...);

// after limeade_init, to make the initial network connection:
int limeade_connect(struct limeade_context *ctx);
// this is done in a separate in case the user wants to hack the library for
// initializing the connection.

// to deconstruct a struct limeade_context:
void limeade_deconstruct(struct limeade_context *ctx);



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
  LIMEADE_PACKET_CLOSE         // connection deinitialization
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
  union
  {
    void *pkt;   // pointer to beginning of packet
    void *flags; // pointer to thing that is also the beginning of the packet
  };
  void *data; // where the flags stop and the fields begin
};

// every packet starts with a magic
#define LIMEADE_MAGIC_RAW "\xFF\xFFLIME^0^\xFF\xFF"
static char LIMEADE_MAGIC[sizeof(LIMEADE_MAGIC_RAW)];

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
  char *distro;      // distrobution of the client machine
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
  struct limeade_indiv_event **events;
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
  struct limeade_indiv_proc **procs;
};

// equivalent to LIMEADE_PACKET_PROC_UPDATe
struct limeade_proc_update
{
  uint16_t total_died;                 // total dead processes
  uint16_t total_altered;              // total altered processes
  pid_t *died;                         // list of dead processes
  struct limeade_indiv_proc **altered; // data of altered processes
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
  LIMEADE_COMMANDEER_WO_JAIL = 1,   // otherwise, executed in jail
  LIMEADE_COMMANDEER_HIGH_PRIV = 2, // otherwise, executed with normal privledges
  LIMEADE_COMMANDEER_TTY = 4,       // otherwise, executed without TTY environment
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

// struct limeade_recv
// received data before parsing
typedef struct limeade_packet_data limeade_recv;

// to receive packets, there are various functions that do nearly the same
// thing.

// holds indefinitely until a packet is received
struct limeade_recv limeade_recv_wait(struct limeade_context *ctx);

// waits for a packet for `hold_time_s` seconds
struct limeade_recv limeade_recv_fixed(struct limeade_context *ctx, int hold_time_s);

// flags can be extracted as so:
struct limeade_packet_flags limeade_parse_flags(struct limeade_recv data);

// for the packet's body, there is a function for each packet type, and the
// type should be checked before parsing:
//
//struct limeade_recv pkt = limeade_recv_wait(ctx);
//
//switch(pkt.type)
//{
//  case LIMEADE_PACKET_KNOCK:
//    struct limeade_knock data = limeade_parse_knock(pkt);
//  --SNIP--
//  case LIMEADE_PACKET_RECOGNIZE:
//    struct limeade_recognize data = limeade_parse_recognize(pkt);
//  --SNIP--
//}
//
// note that if the wrong function is called for parsing, it will detect it
// and push a LIMEADE_ERROR_GARBAGE and return NULL
struct limeade_knock limeade_parse_knock(struct limeade_recv pkt);
struct limeade_recognize limeade_parse_recognize(struct limeade_recv pkt);
struct limeade_intro limeade_parse_introduction(struct limeade_recv pkt);
#define limeade_parse_intro(x) limeade_parse_introduction(x)
struct limeade_ack limeade_parse_acknowledge(struct limeade_recv pkt);
#define limeade_parse_ack(x) limeade_parse_acknowledge(x)
struct limeade_events limeade_parse_events(struct limeade_recv pkt);
struct limeade_proc_generic limeade_parse_proc_generic(struct limeade_recv pkt);
struct limeade_proc_update limeade_parse_proc_update(struct limeade_recv pkt);
struct limeade_perf limeade_parse_perf(struct limeade_recv pkt);
struct limeade_commandeer limeade_parse_commandeer(struct limeade_recv pkt);
struct limeade_exited limeade_parse_exited(struct limeade_recv pkt);
struct limeade_close limeade_parse_close(struct limeade_recv pkt);



/*** *** INTERNAL *** ***/
// the below functions exist not for the end user, but for use interally
// by the library
void limeade_deflate_packet(struct limeade_packet_data *in, int compr_lvl);
void limeade_monotonic(struct timespec *ts);

#endif /* _LIBLIMEADE_H_ */
