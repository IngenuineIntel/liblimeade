# liblimeade tests

This directory contains:

- `procs_selftest.c`: a multithreaded self-loopback process-data test.
- `configure_ssh_subsystem.sh`: OpenSSH subsystem setup helper.

## What the test validates

`procs_selftest.c` exercises three requirements in one run:

1. send process data to itself
2. receive process data from itself
3. display process data

It runs a host and client in the same process with `pthread`s, sends a synthetic `CLIENT_PROCS_GENERIC` payload over loopback SSH, receives it on the other side, and prints decoded rows to stdout.

## Prerequisites

Install development dependencies (package names may vary by distro):

- `gcc`
- `make` (optional)
- `pkg-config`
- `libssh` development headers (`libssh-dev` / `libssh-devel`)
- `zlib` development headers (`zlib1g-dev` / `zlib-devel`)

Also ensure the local user has a usable SSH keypair, because `limeade_client_init` uses public-key auth:

```bash
ssh-keygen -t ed25519 -N "" -f ~/.ssh/id_ed25519
```

## Build the test

From repository root:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic \
  -Iinclude \
  tests/procs_selftest.c src/ssh-compat.c src/errors.c \
  -o tests/procs_selftest \
  -lssh -lpthread
```

## Run the test

From repository root:

```bash
./tests/procs_selftest
```

Expected high-level output includes:

- loopback context initialization
- send/receive byte counts
- displayed process rows table
- success completion message

## Configure OpenSSH subsystem (optional but recommended)

For deployments that route limeade traffic through system `sshd`, configure a subsystem entry:

Dry-run preview:

```bash
bash tests/configure_ssh_subsystem.sh
```

Apply changes (root):

```bash
sudo bash tests/configure_ssh_subsystem.sh --apply --subsystem-bin /usr/local/libexec/limeade-subsystem
```

This writes:

- `/etc/ssh/sshd_config.d/99-limeade-subsystem.conf`

with a line:

- `Subsystem limeade /usr/local/libexec/limeade-subsystem`

Then validates `sshd` config and reloads/restarts SSH service when available.
