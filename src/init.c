// init.c
// initialization and shutdown for liblimeade
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

#include <signal.h>
#include <unistd.h>

#include <liblimeade/liblimeade.h>

#define CHECK(expr, err)          \
{                                 \
  if(expr)                        \
  {                               \
    limeade_inserror(err);        \
    ctx = (LIMEADE_CONTEXT*)NULL; \
    return *ctx;                   \
  }                               \
}

LIMEADE_CONTEXT limeade_client_init(uint16_t port, const char *dest)
{
  /* There are two ways to do this
   * 1: libssh
   * utilize libssh to create a connection, and secure from that connection
   * file descriptors needed to send and recv

   * 2: pipe/dup/fork/exec
   * spawn an SSH process that initializes the connection and subsystem on the
   * host, and pipe the file descriptors for send/recv from that processes, and
   * kill it when done
   *
   * pros of 1:
   *  - memory efficiency
   * pros of 2:
   *  - it's how the SFTP client is implemented
   *  - pretty programmatically simplistic
   *  - optimized from a queueing perspective
   * going with 2 atm
   */
  LIMEADE_CONTEXT *ctx = (LIMEADE_CONTEXT*)calloc(1, sizeof(*ctx));
  CHECK(ctx == NULL, LIMEADE_ERROR_MEMORY);

  int to_ssh[2];
  int from_ssh[2];

  // create pipes
  CHECK(pipe(to_ssh) == -1, LIMEADE_ERROR_OTHER);
  CHECK(pipe(from_ssh) == -1, LIMEADE_ERROR_OTHER);

  pid_t pid = fork();

  if(pid == 0) // child
  {
    // create dups
    CHECK(dup2(to_ssh[0], STDIN_FILENO) == -1, LIMEADE_ERROR_OTHER);
    CHECK(dup2(from_ssh[1], STDOUT_FILENO) == -1, LIMEADE_ERROR_OTHER);

    // close fds
    close(to_ssh[0]);
    close(to_ssh[1]);
    close(from_ssh[0]);
    close(from_ssh[1]);

    execlp("ssh", "ssh", "-s", dest, LIMEADE_SUBSYSTEM_NAME, (char*)NULL);
    exit(-1);

  }

  close(to_ssh[0]);
  close(from_ssh[1]);

  ctx->recv_fd = from_ssh[0];
  ctx->send_fd = to_ssh[1];
  ctx->ssh_child_pid = pid;
  ctx->role = LIMEADE_ROLE_HOST;

  return *ctx;

}

LIMEADE_CONTEXT limeade_host_init()
{
  LIMEADE_CONTEXT *ctx = (LIMEADE_CONTEXT*)calloc(1, sizeof(*ctx));

  // send/recv fds are passed to SSH subsystems as stdin & stdout
  ctx->send_fd = 1; // STDOUT
  ctx->recv_fd = 0; // STDIN
  ctx->role = LIMEADE_ROLE_CLIENT;
  //ctx.ssh_child_pid = 0; // not used by the host

  return *ctx;

}

void limeade_host_free(LIMEADE_CONTEXT *self)
{
  // kill SSH child process
  kill(self->ssh_child_pid, SIGKILL);

  // likewise, close fds
  close(self->recv_fd);
  close(self->send_fd);

  // free context
  free((void*)self);
}

void limeade_client_free(LIMEADE_CONTEXT *self)
{
  close(self->recv_fd);
  close(self->send_fd);

  free((void*)self);
}

#undef CHECK
