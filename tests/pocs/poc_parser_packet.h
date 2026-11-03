#ifndef LIMEADE_POC_PARSER_PACKET_H
#define LIMEADE_POC_PARSER_PACKET_H

#include<arpa/inet.h>
#include<stdlib.h>
#include<string.h>
#include<sys/socket.h>
#include<unistd.h>

#include<liblimeade/liblimeade.h>

static inline int poc_receive_packet(enum limeade_packet type,
                                     uint8_t compression_level,
                                     const unsigned char *body,
                                     size_t body_size,
                                     struct limeade_context *receiver,
                                     struct limeade_recvd *packet)
{
  struct sockaddr_in address;
  socklen_t address_size = sizeof(address);
  size_t packet_size = sizeof(LIMEADE_MAGIC)
                     + sizeof(struct limeade_packet_flags) + body_size;
  unsigned char *wire = calloc(1, packet_size);
  int sender;
  int result;

  if(!wire)
    return LIMEADE_ERROR_MEMORY;
  result = limeade_init(receiver,
      LIMEADE_MODE_HOST_ETH | LIMEADE_MODE_NO_COMPRESSION, 0);
  if(result != LIMEADE_SUCCESS)
  {
    free(wire);
    return result;
  }
  result = limeade_connect(receiver);
  if(result != LIMEADE_SUCCESS)
  {
    free(wire);
    return result;
  }
  if(getsockname(receiver->rfd, (struct sockaddr*)&address, &address_size) != 0)
  {
    free(wire);
    limeade_destruct(receiver);
    return LIMEADE_ERROR_NETWORK;
  }

  memcpy(wire, LIMEADE_MAGIC, sizeof(LIMEADE_MAGIC));
  struct limeade_packet_flags *flags =
      (struct limeade_packet_flags*)(wire + sizeof(LIMEADE_MAGIC));
  flags->type = type;
  flags->compr_lvl = compression_level;
  flags->packet_size = packet_size;
  memcpy(wire + sizeof(LIMEADE_MAGIC) + sizeof(*flags), body, body_size);

  sender = socket(AF_INET, SOCK_DGRAM, 0);
  if(sender < 0)
  {
    free(wire);
    limeade_destruct(receiver);
    return LIMEADE_ERROR_NETWORK;
  }
  result = (int)sendto(sender, wire, packet_size, 0,
                       (struct sockaddr*)&address, address_size);
  close(sender);
  free(wire);
  if(result < 0)
  {
    limeade_destruct(receiver);
    return LIMEADE_ERROR_NETWORK;
  }

  result = limeade_recv_wait_noreply(receiver, packet, 5000);
  if(result != LIMEADE_SUCCESS)
    limeade_destruct(receiver);
  return result;
}

#endif