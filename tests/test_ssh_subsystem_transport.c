// test_ssh_subsystem_transport.c
// Tests for SSH-subsystem transport API and framed packet helpers.

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <libssh/libssh.h>
#include <liblimeade.h>

static int test_framed_roundtrip_over_fds(void)
{
	int sv[2];
	LIMEADE_CONTEXT send_ctx;
	LIMEADE_CONTEXT recv_ctx;
	unsigned char payload[] = {0x10, 0x20, 0x30, 0x40, 0x50};
	LIMEADE_PACKET got;

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0)
	{
		perror("socketpair");
		return 1;
	}

	send_ctx.send = sv[0];
	send_ctx.recv = -1;
	send_ctx.session = NULL;
	send_ctx.channel = NULL;
	send_ctx.mode = LIMEADE_CONTEXT_HOST_SUBSYSTEM;

	recv_ctx.send = -1;
	recv_ctx.recv = sv[1];
	recv_ctx.session = NULL;
	recv_ctx.channel = NULL;
	recv_ctx.mode = LIMEADE_CONTEXT_HOST_SUBSYSTEM;

	if (limeade_host_send(send_ctx, payload, sizeof(payload)) != 0)
	{
		fprintf(stderr, "limeade_host_send failed\n");
		close(sv[0]);
		close(sv[1]);
		return 1;
	}

	got = limeade_host_recv(recv_ctx);
	if (got.data == NULL || got.sz != sizeof(payload))
	{
		fprintf(stderr, "limeade_host_recv size/data mismatch\n");
		close(sv[0]);
		close(sv[1]);
		free(got.data);
		return 1;
	}

	if (memcmp(got.data, payload, sizeof(payload)) != 0)
	{
		fprintf(stderr, "payload mismatch\n");
		close(sv[0]);
		close(sv[1]);
		free(got.data);
		return 1;
	}

	free(got.data);
	close(sv[0]);
	close(sv[1]);
	return 0;
}

static int test_optional_ssh_subsystem_client_init(void)
{
	const char *user = getenv("LIMEADE_TEST_SSH_USER");
	const char *passwd = getenv("LIMEADE_TEST_SSH_PASSWORD");
	const char *port_s = getenv("LIMEADE_TEST_SSH_PORT");
	unsigned int port = 22;
	LIMEADE_CONTEXT ctx;

	if (user == NULL)
	{
		printf("SKIP: set LIMEADE_TEST_SSH_USER to run SSH subsystem init smoke test\n");
		return 0;
	}

	if (port_s != NULL && port_s[0] != '\0')
	{
		port = (unsigned int)strtoul(port_s, NULL, 10);
	}

	ctx = limeade_client_init(port, user, passwd);
	if (ctx.channel == NULL || ctx.session == NULL)
	{
		fprintf(stderr, "limeade_client_init did not establish SSH subsystem channel\n");
		return 1;
	}

	// Cleanup libssh handles created by limeade_client_init.
	// Context stores opaque pointers to ssh_session/ssh_channel.
	{
		ssh_channel channel = (ssh_channel)ctx.channel;
		ssh_session session = (ssh_session)ctx.session;
		ssh_channel_send_eof(channel);
		ssh_channel_close(channel);
		ssh_channel_free(channel);
		ssh_disconnect(session);
		ssh_free(session);
	}

	return 0;
}

int main(void)
{
	int rc = 0;

	rc |= test_framed_roundtrip_over_fds();
	rc |= test_optional_ssh_subsystem_client_init();

	if (rc == 0)
	{
		printf("PASS: ssh subsystem transport tests\n");
	}

	return rc;
}
