#include<stdio.h>
#include<string.h>

#include<liblimeade/liblimeade.h>
#include"poc_parser_packet.h"

static int parse_bad_record(const char *kind)
{
  unsigned char events[] = {0x22, 1, 0, LIMEADE_ROW_DELIM, 0x40};
  unsigned char process[128];
  unsigned char *body = events;
  size_t body_size = sizeof(events);
  enum limeade_packet type;
  struct limeade_context receiver = {0};
  struct limeade_recvd packet = {0};
  int result;

  if(strcmp(kind, "events") == 0)
  {
    type = LIMEADE_PACKET_EVENTS;
  } else if(strcmp(kind, "generic") == 0)
  {
    size_t pos = 0;
    process[pos++] = 0;
    process[pos++] = LIMEADE_FIELD_DELIM;
    process[pos++] = 0;
    process[pos++] = LIMEADE_FIELD_DELIM;
    process[pos++] = 0x22;
    process[pos++] = 1;
    process[pos++] = 0;
    process[pos++] = LIMEADE_ROW_DELIM;
    for(int i = 0; i < 15; i++)
    {
      process[pos++] = 0;
      process[pos++] = LIMEADE_FIELD_DELIM;
    }
    process[pos++] = 'R';
    process[pos++] = LIMEADE_FIELD_DELIM;
    process[pos++] = 'n';
    process[pos++] = LIMEADE_FIELD_DELIM;
    process[pos++] = 'x';
    body = process;
    body_size = pos;
    type = LIMEADE_PACKET_PROC_GENERIC;
  } else if(strcmp(kind, "update") == 0)
  {
    size_t pos = 0;
    process[pos++] = 0;
    process[pos++] = LIMEADE_FIELD_DELIM;
    process[pos++] = 0;
    process[pos++] = LIMEADE_FIELD_DELIM;
    process[pos++] = 0;
    process[pos++] = LIMEADE_FIELD_DELIM;
    process[pos++] = 0x22;
    process[pos++] = 1;
    process[pos++] = 0;
    process[pos++] = LIMEADE_ROW_DELIM;
    process[pos++] = LIMEADE_ROW_DELIM;
    for(int i = 0; i < 15; i++)
    {
      process[pos++] = 0;
      process[pos++] = LIMEADE_FIELD_DELIM;
    }
    process[pos++] = 'R';
    process[pos++] = LIMEADE_FIELD_DELIM;
    process[pos++] = 'n';
    process[pos++] = LIMEADE_FIELD_DELIM;
    process[pos++] = 'x';
    body = process;
    body_size = pos;
    type = LIMEADE_PACKET_PROC_UPDATE;
  } else
  {
    fprintf(stderr, "usage: %s events|generic|update\n", "poc_parser_cleanup");
    return 2;
  }

  int receive_result = poc_receive_packet(type, 0, body, body_size,
                                          &receiver, &packet);
  if(receive_result != LIMEADE_SUCCESS)
  {
    fprintf(stderr, "library receive failed: %d\n", receive_result);
    return 1;
  }

  if(type == LIMEADE_PACKET_EVENTS)
  {
    struct limeade_events out = {0};
    result = limeade_parse_events(&out, &packet);
  } else if(type == LIMEADE_PACKET_PROC_GENERIC)
  {
    struct limeade_proc_generic out = {0};
    result = limeade_parse_proc_generic(&out, &packet);
  } else
  {
    struct limeade_proc_update out = {0};
    result = limeade_parse_proc_update(&out, &packet);
  }

  printf("parser returned %d for malformed %s record\n", result, kind);
  limeade_release_recvd(&packet);
  limeade_destruct(&receiver);
  return 0;
}

int main(int argc, char **argv)
{
  if(argc != 2)
  {
    fprintf(stderr, "usage: %s events|generic|update\n", argv[0]);
    return 2;
  }
  return parse_bad_record(argv[1]);
}