// TEST 1
// THE TEST THAT IS THE FIRST
// THE TEST THAT IS THE NUMBER 1 TEST
// THE FIRST TEST OF TESTS
// THE TEST THAT IS THE 1 THAT IS THE FIRST 1
// THE NO 1 1 NO 1 TEST

#include<liblimeade/liblimeade.h>
#define test(e) printf("%s\n", LIMEADE_ERROR_REPRS[e]);
int main(int argc, char **argv)
{
  struct limeade_context c;
  struct limeade_indiv_event v, ev[3];
  struct limeade_events p;

  int e = limeade_init(&c, LIMEADE_MODE_HOST_SSH|LIMEADE_MODE_NO_COMPRESSION);

  c.compr_lvl = 2;
  e = limeade_connect(&c);
/*
  p.hostname = "arch";
  p.kernelver = "7.2.0-arch";
  p.distro    = "arch btw";
  p.origin_user = "roan";
  p.processor   = "i7-7700";
  p.vendor      = "IngenuineIntel";
  p.ram_mbs     = 16 * 1024;
  p.swap_mbs    = 20 * 1024;

  e = limeade_send(&c, LIMEADE_PACKET_INTRO, p);
*/

  v.ts_s = 0x69696969;
  v.ts_ms = 0x6767;
  v.pid = 420;
  v.syscall = "callin' deez nuts!";
  v.arg1    = "argin' deez nuts!";
  v.arg2    = "just give him the maalk, Josh!";
  v.retval = 69420;


  ev[0] = ev[1] = ev[2] = v;

  p.nr_events = 3;
  p.events = ev;

  limeade_send(&c, LIMEADE_PACKET_EVENTS, p);

  return 0;
}
