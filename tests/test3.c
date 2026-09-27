// THE TEST THAT IS THE THIRD
// THE NO 1 NO 3 TEST
// ALSO THE NO 3 NO 1 TEST

#include<unistd.h>

#include<liblimeade/liblimeade.h>

#define E(m, e) limeade_perror(m, e);
int main()
{
  struct limeade_context client, host;
  struct limeade_recvd recvd;
  struct limeade_proc_update pkt1, pkt2;
  int e;
 
  e = limeade_init(&host, LIMEADE_MODE_HOST_ETH|LIMEADE_MODE_NO_COMPRESSION);
  E("host init", e);
 
  e = limeade_init(&client, LIMEADE_MODE_CLIENT_ETH|LIMEADE_MODE_LOW_COMPRESSION);
  E("client init", e);

  e = limeade_connect(&host);
  E("host connect", e);

  e = limeade_connect(&client);
  E("client connect", e);

  // TODO generate _valid_ packet in `pkt` that simulates actual process info
  
  e = limeade_send(&client, LIMEADE_PACKET_PROC_UPDATE, &pkt1);
  E("client send", e);

  e = limeade_recv(&host, &recvd);
  E("host receive", e);

  e = limeade_parse_proc_update(&pkt2, recvd);
  E("host parse", e);

  // TODO check pkt2 against pkt1

  //write(STDOUT_FILENO, recvd.pkt, recvd.pkt_sz);
  
  return 0;
}

