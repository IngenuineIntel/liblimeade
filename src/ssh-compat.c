// ssh-compat.c
// SSH compatibility and I/O context management
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

#include <liblimeade.h>

#include <arpa/inet.h>
#include <libssh/libssh.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

static const char *LIMEADE_SUBSYSTEM_NAME = "limeade";

static int limeade_write_all_fd(int fd, const void *buf, size_t len)
{
	const unsigned char *p = (const unsigned char *)buf;

	if (fd < 0 || (p == NULL && len > 0))
	{
		return -1;
	}

	while (len > 0)
	{
		ssize_t n = write(fd, p, len);
		if (n <= 0)
		{
			return -1;
		}
		p += (size_t)n;
		len -= (size_t)n;
	}

	return 0;
}

static int limeade_read_all_fd(int fd, void *buf, size_t len)
{
	unsigned char *p = (unsigned char *)buf;

	if (fd < 0 || (p == NULL && len > 0))
	{
		return -1;
	}

	while (len > 0)
	{
		ssize_t n = read(fd, p, len);
		if (n <= 0)
		{
			return -1;
		}
		p += (size_t)n;
		len -= (size_t)n;
	}

	return 0;
}

static int limeade_write_all_channel(ssh_channel channel, const void *buf, size_t len)
{
	const unsigned char *p = (const unsigned char *)buf;

	if (channel == NULL || (p == NULL && len > 0))
	{
		return -1;
	}

	while (len > 0)
	{
		int n = ssh_channel_write(channel, p, len);
		if (n <= 0)
		{
			return -1;
		}
		p += (size_t)n;
		len -= (size_t)n;
	}

	return 0;
}

static int limeade_read_all_channel(ssh_channel channel, void *buf, size_t len)
{
	unsigned char *p = (unsigned char *)buf;

	if (channel == NULL || (p == NULL && len > 0))
	{
		return -1;
	}

	while (len > 0)
	{
		int n = ssh_channel_read(channel, p, len, 0);
		if (n <= 0)
		{
			return -1;
		}
		p += (size_t)n;
		len -= (size_t)n;
	}

	return 0;
}

static int limeade_send_framed(LIMEADE_CONTEXT ctx, void *pckt, size_t sz)
{
	uint32_t net_sz;
	ssh_channel channel;

	if ((pckt == NULL && sz > 0) || sz > UINT32_MAX)
	{
		return -1;
	}

	net_sz = htonl((uint32_t)sz);

	if (ctx.channel != NULL)
	{
		channel = (ssh_channel)ctx.channel;
		if (limeade_write_all_channel(channel, &net_sz, sizeof(net_sz)) != 0)
		{
			return -1;
		}
		if (sz > 0 && limeade_write_all_channel(channel, pckt, sz) != 0)
		{
			return -1;
		}
		return 0;
	}

	if (limeade_write_all_fd(ctx.send, &net_sz, sizeof(net_sz)) != 0)
	{
		return -1;
	}
	if (sz > 0 && limeade_write_all_fd(ctx.send, pckt, sz) != 0)
	{
		return -1;
	}

	return 0;
}

static LIMEADE_PACKET limeade_recv_framed(LIMEADE_CONTEXT ctx)
{
	LIMEADE_PACKET out = {0, NULL};
	uint32_t net_sz;
	ssh_channel channel;

	if (ctx.channel != NULL)
	{
		channel = (ssh_channel)ctx.channel;
		if (limeade_read_all_channel(channel, &net_sz, sizeof(net_sz)) != 0)
		{
			return out;
		}
	}
	else
	{
		if (limeade_read_all_fd(ctx.recv, &net_sz, sizeof(net_sz)) != 0)
		{
			return out;
		}
	}

	out.sz = ntohl(net_sz);
	if (out.sz == 0)
	{
		return out;
	}

	out.data = (unsigned char *)malloc(out.sz);
	if (out.data == NULL)
	{
		out.sz = 0;
		return out;
	}

	if (ctx.channel != NULL)
	{
		channel = (ssh_channel)ctx.channel;
		if (limeade_read_all_channel(channel, out.data, out.sz) != 0)
		{
			free(out.data);
			out.data = NULL;
			out.sz = 0;
		}
	}
	else
	{
		if (limeade_read_all_fd(ctx.recv, out.data, out.sz) != 0)
		{
			free(out.data);
			out.data = NULL;
			out.sz = 0;
		}
	}

	return out;
}

LIMEADE_CONTEXT limeade_host_init(unsigned int port)
{
	LIMEADE_CONTEXT ctx = {-1, -1, NULL, NULL, LIMEADE_CONTEXT_HOST_SUBSYSTEM};

	(void)port;

	// In SSH subsystem mode, sshd wires stdin/stdout to the channel stream.
	ctx.recv = STDIN_FILENO;
	ctx.send = STDOUT_FILENO;
	return ctx;
}

LIMEADE_CONTEXT limeade_client_init(unsigned int port, const char *user, const char *passwd)
{
	LIMEADE_CONTEXT ctx = {-1, -1, NULL, NULL, LIMEADE_CONTEXT_CLIENT_SUBSYSTEM};
	ssh_session session = NULL;
	ssh_channel channel = NULL;
	int rc;

	if (user == NULL)
	{
		return ctx;
	}

	session = ssh_new();
	if (session == NULL)
	{
		return ctx;
	}

	rc = ssh_options_set(session, SSH_OPTIONS_HOST, "127.0.0.1");
	if (rc != SSH_OK)
	{
		goto fail;
	}

	rc = ssh_options_set(session, SSH_OPTIONS_PORT, &port);
	if (rc != SSH_OK)
	{
		goto fail;
	}

	rc = ssh_options_set(session, SSH_OPTIONS_USER, user);
	if (rc != SSH_OK)
	{
		goto fail;
	}

	rc = ssh_connect(session);
	if (rc != SSH_OK)
	{
		goto fail;
	}

	if (passwd != NULL)
	{
		rc = ssh_userauth_password(session, NULL, passwd);
	}
	else
	{
		rc = ssh_userauth_publickey_auto(session, NULL, NULL);
	}
	if (rc != SSH_AUTH_SUCCESS)
	{
		goto fail;
	}

	channel = ssh_channel_new(session);
	if (channel == NULL)
	{
		goto fail;
	}

	rc = ssh_channel_open_session(channel);
	if (rc != SSH_OK)
	{
		goto fail;
	}

	rc = ssh_channel_request_subsystem(channel, LIMEADE_SUBSYSTEM_NAME);
	if (rc != SSH_OK)
	{
		goto fail;
	}

	ctx.session = (void *)session;
	ctx.channel = (void *)channel;
	return ctx;

fail:
	if (channel != NULL)
	{
		ssh_channel_close(channel);
		ssh_channel_free(channel);
	}
	if (session != NULL)
	{
		ssh_disconnect(session);
		ssh_free(session);
	}
	return ctx;
}

int limeade_host_send(LIMEADE_CONTEXT ctx, void *pckt, size_t sz)
{
	return limeade_send_framed(ctx, pckt, sz);
}

int limeade_client_send(LIMEADE_CONTEXT ctx, void *pckt, size_t sz)
{
	return limeade_send_framed(ctx, pckt, sz);
}

LIMEADE_PACKET limeade_host_recv(LIMEADE_CONTEXT ctx)
{
	return limeade_recv_framed(ctx);
}

LIMEADE_PACKET limeade_client_recv(LIMEADE_CONTEXT ctx)
{
	return limeade_recv_framed(ctx);
}


