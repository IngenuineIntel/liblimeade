// THE TEST THAT IS THE NO 1 NO 2 TEST
// THE THAT THAT ALSO IS THE NO 2 NO 1 TEST
// THE TEST THAT IS THE SECOND FIRST
// THE TEST THAT IS THE SECOND, FIRST
// anyway

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>

#include<liblimeade/liblimeade.h>

#define NOT_OK(e) if(e != LIMEADE_OK) goto cleanup;
#define limeade_perror(a, b) do{}while(0);
int main(int argc, char **argv)
{
  struct limeade_context host, client;
  struct limeade_indiv_proc altered;
  struct limeade_proc_update update, parsed;
  struct limeade_recvd received;
  int host_ready, client_ready, parsed_ready;
  int result = EXIT_FAILURE;
  int e;

  host_ready = client_ready = parsed_ready = 0;

  e = limeade_init(&host, LIMEADE_MODE_HOST_ETH|LIMEADE_MODE_NO_COMPRESSION,
                       LIMEADE_PORT);
  limeade_perror("host init", e);
  NOT_OK(e);

  e = limeade_connect(&host);
  limeade_perror("host connect", e);
  NOT_OK(e);

  host_ready = 1;

  e = limeade_init(&client, LIMEADE_MODE_CLIENT_ETH|LIMEADE_MODE_NO_COMPRESSION,
                   "127.0.0.1", LIMEADE_PORT);
  limeade_perror("client init", e);
  NOT_OK(e);

  client.compr_lvl = 9;

  e = limeade_connect(&client);
  limeade_perror("client connect", e);
  NOT_OK(e);
  
  client_ready = 1;

  altered.pid = getpid();
  altered.ppid = getppid();
  altered.uid = getuid();
  altered.threads = 1;
  altered.cpu_ticks = 1234;
  altered.ram_kb = 5678;
  altered.command = "limeade-test2";

  update.total_died = 0;
  update.total_altered = 1;
  update.died = NULL;
  update.altered = &altered;

  e = limeade_send(&client, LIMEADE_PACKET_PROC_UPDATE, update);
  limeade_perror("client send", e);
  NOT_OK(e);

  e = limeade_recv_wait_noreply(&host, &received, 3000);
  limeade_perror("host recv", e);
  NOT_OK(e);

  write(STDOUT_FILENO, received.pkt, received.pkt_sz);

  e = limeade_parse_proc_update(&parsed, received);
  limeade_perror("parse", e);
  NOT_OK(e);
  
  parsed_ready = 1;

  if(parsed.total_died != 0 || parsed.total_altered != 1 ||
     parsed.altered[0].pid != altered.pid ||
     parsed.altered[0].ppid != altered.ppid ||
     parsed.altered[0].uid != altered.uid ||
     parsed.altered[0].threads != altered.threads ||
     parsed.altered[0].cpu_ticks != altered.cpu_ticks ||
     parsed.altered[0].ram_kb != altered.ram_kb ||
     strcmp(parsed.altered[0].command, altered.command) != 0)
  {
    fprintf(stderr, "parsed update did not match the sent data\n");
    goto cleanup;
  }

  //printf("process update round trip passed\n\n");

  result = EXIT_SUCCESS;

cleanup:
  if(parsed_ready)
    limeade_release(&parsed);
  limeade_release(&received);
  if(client_ready)
    limeade_destruct(&client);
  if(host_ready)
    limeade_destruct(&host);
  return result;
}

