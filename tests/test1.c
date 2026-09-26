// TEST 1
// THE TEST THAT IS THE FIRST
// THE TEST THAT IS THE NUMBER 1 TEST
// THE FIRST TEST OF TESTS
// THE TEST THAT IS THE 1 THAT IS THE FIRST 1
// THE NO 1 1 NO 1 TEST

#include<pthread.h>
#include<stdio.h>
#include<unistd.h>

#include<liblimeade/liblimeade.h>
#define test(e) printf("%s\n", LIMEADE_ERROR_REPRS[e]);
int main(int argc, char **argv)
{
  struct limeade_context c;
  struct limeade_indiv_event v, ev[6];
  struct limeade_events p;
  int fds[2];

  //printf("limeade_init...\n");
  int e = limeade_init(&c, LIMEADE_MODE_HOST_SSH|LIMEADE_MODE_NO_COMPRESSION);
  //test(e);

  c.compr_lvl = 0;

  pipe(fds);
  c.sfd = fds[1];
  c.rfd = fds[0];

  //printf("limeade_connect...\n");
  e = limeade_connect(&c);
  //test(e);

  v.ts_s = 0x69696969;
  v.ts_ms = 0x6767;
  v.pid = 420;
  v.syscall = "callin' deez nuts!";
  v.arg1    = "argin' deez nuts!";
  v.arg2    = "just give him the maalk, Josh!";
  v.retval = 69420;

  ev[0] = ev[1] = ev[2] = v;

  v.ts_s = 0xDEADBEEF;
  v.ts_ms = 0xC0DE;
  v.pid   = 69420;
  v.syscall = "Syscall? I barely know'er!";
  v.arg1    = "Remember when he said it was arg time...";
  v.arg2    = "...and proceeded to arg all over the place?";
  v.retval  = 0;

  ev[3] = ev[4] = ev[5] = v;

  p.nr_events = 6;
  p.events = ev;

  //printf("limeade_send...\n");
  e = limeade_send(&c, LIMEADE_PACKET_EVENTS, p);
  //test(e);

  struct limeade_recvd r;
  //printf("limeade_recv_noreply...\n");
  e = limeade_recv_wait_noreply(&c, &r, 3000);
  //test(e);
  
  //printf("r.pkt_sz = %i\n", r.pkt_sz);
  write(STDOUT_FILENO, r.pkt, r.pkt_sz);

  limeade_release(&r);

  limeade_destruct(&c);

  return 0;
}

