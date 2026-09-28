// THE TEST THAT IS THE THIRD
// THE NO 1 NO 3 TEST
// ALSO THE NO 3 NO 1 TEST

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>

#include<liblimeade/liblimeade.h>

#define E(m, e) /*if(e != LIMEADE_SUCCESS)*/ limeade_perror((m), (e));
#define COMPR_LVL(ctx) printf("%i\n", ctx.compr_lvl);
static int proc_update_matches(const struct limeade_proc_update *a,
                               const struct limeade_proc_update *b)
{
  if(a->total_died != b->total_died || a->total_altered != b->total_altered)
    return 0;

  for(int i = 0; i < a->total_died; i++)
    if(a->died[i] != b->died[i])
      return 0;

  for(int i = 0; i < a->total_altered; i++)
  {
    const struct limeade_indiv_proc *x = &a->altered[i];
    const struct limeade_indiv_proc *y = &b->altered[i];

    if(x->pid != y->pid || x->ppid != y->ppid || x->uid != y->uid ||
       x->threads != y->threads || x->cpu_ticks != y->cpu_ticks ||
       x->ram_kb  != y->ram_kb  || strcmp(x->command, y->command) != 0)
      return 0;
  }

  return 1;
}

int main(void)
{
  const int nr = 512;
  struct limeade_context client, host;
  struct limeade_recvd recvd = {0};
  struct limeade_proc_update pkt1 = {0}, pkt2 = {0};
  struct limeade_indiv_proc *altered;
  pid_t *died;
  int e;

  altered = malloc(sizeof(*altered) * nr);
  died = malloc(sizeof(*died));

  died[0] = 9999;

  for(int i = 0; i < nr; i++)
  {
    char name[256];
    snprintf(name, sizeof(name), "limeade-test3-%03d-%s-%s-%s",
             i, "0123456789abcdef", "fedcba9876543210", "abcdefghijklmnop");
    altered[i].pid = 1000 + i;
    altered[i].ppid = 2000 + i;
    altered[i].uid = 3000 + i;
    altered[i].threads = 1 + (i % 4);
    altered[i].cpu_ticks = 1234 + i;
    altered[i].ram_kb = 4096 + i;
    altered[i].command = strdup(name);
    if(!altered[i].command)
    {
      for(int j = 0; j < i; j++)
        free(altered[j].command);
      free(altered);
      free(died);
      return 1;
    }
  }

  pkt1.total_died = 1;
  pkt1.total_altered = nr;
  pkt1.died = died;
  pkt1.altered = altered;

  if(!proc_update_matches(&pkt1, &pkt1))
  {
    fprintf(stderr, "pre-send validation failed\n");
    for(int i = 0; i < nr; i++)
      free(altered[i].command);
    free(altered);
    free(died);
    return 1;
  }

  e = limeade_init(&host, LIMEADE_MODE_HOST_ETH|LIMEADE_MODE_NO_COMPRESSION,
                   LIMEADE_PORT);
  E("host init", e);

  e = limeade_init(&client, LIMEADE_MODE_CLIENT_ETH|LIMEADE_MODE_HIGH_COMPRESSION,
                   "127.0.0.1", LIMEADE_PORT);
  E("client init", e);
  COMPR_LVL(client);

  e = limeade_connect(&host);
  E("host connect", e);

  e = limeade_connect(&client);
  E("client connect", e);
  COMPR_LVL(client);

  e = limeade_send(&client, LIMEADE_PACKET_PROC_UPDATE, pkt1);
  E("client send", e);

  e = limeade_recv_wait_noreply(&host, &recvd, 3000);
  E("host receive", e);
  if(e != LIMEADE_SUCCESS)
  {
    fprintf(stderr, "receive returned no packet; aborting test\n");
    limeade_destruct(&client);
    limeade_destruct(&host);
    for(int i = 0; i < nr; i++)
      free(altered[i].command);
    free(altered);
    free(died);
    return 1;
  }

  printf("sz = %i\n", recvd.pkt_sz);
  //write(STDOUT_FILENO, recvd.pkt, recvd.pkt_sz);
  e = limeade_parse_proc_update(&pkt2, &recvd);

  //write(STDOUT_FILENO, recvd.pkt, recvd.pkt_sz);
  printf("sz = %i\n", recvd.pkt_sz);
  //if(recvd.pkt_sz <= 15 * 1024)
  //  fprintf(stderr, "packet is too small: %u bytes\n", recvd.pkt_sz);

  E("host parse", e);
  if(e != LIMEADE_SUCCESS)
  {
    fprintf(stderr, "parse failed: %d\n", e);
    limeade_release(&recvd);
    limeade_destruct(&client);
    limeade_destruct(&host);
    for(int i = 0; i < nr; i++)
      free(altered[i].command);
    free(altered);
    free(died);
    return 1;
  }

  if(!proc_update_matches(&pkt1, &pkt2))
  {
    fprintf(stderr, "post-parse validation failed\n");
    limeade_release(&pkt2);
    limeade_release(&recvd);
    limeade_destruct(&client);
    limeade_destruct(&host);
    for(int i = 0; i < nr; i++)
      free(altered[i].command);
    free(altered);
    free(died);
    return 1;
  }

  limeade_release(&pkt2);
  limeade_release(&recvd);
  limeade_destruct(&client);
  limeade_destruct(&host);

  for(int i = 0; i < nr; i++)
    free(altered[i].command);
  free(altered);
  free(died);

  return 0;
}

