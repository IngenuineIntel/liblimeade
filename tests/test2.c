// THE TEST THAT IS THE NO 1 NO 2 TEST
// THE THAT THAT ALSO IS THE NO 2 NO 1 TEST
// THE TEST THAT IS THE SECOND FIRST
// THE TEST THAT IS THE SECOND, FIRST
// anyway

#include<stdio.h>

#include<liblimeade/liblimeade.h>

#define test(e) printf("%s\n", LIMEADE_ERROR_REPRS[e]);
int main(int argc, char **argv)
{
  struct limeade_context c1, c2;
  struct limeade_acknowledge i, o;
  struct limeade_recvd r;
  int e;

  printf("limeade_init 1...\n");
  e = limeade_init(&c1, LIMEADE_MODE_HOST_ETH|LIMEADE_MODE_NO_COMPRESSION,
                   LIMEADE_PORT);
  test(e);

  printf("limeade_init 2...\n");
  e = limeade_init(&c2, LIMEADE_MODE_CLIENT_ETH|LIMEADE_MODE_NO_COMPRESSION,
                   "127.0.0.1", LIMEADE_PORT);
  test(e);

  printf("limeade_connect 1...\n");
  e = limeade_connect(&c1);
  test(e);

  printf("limeade_connect 2...\n");
  e = limeade_connect(&c2);
  test(e);
  
  return 0;
}

