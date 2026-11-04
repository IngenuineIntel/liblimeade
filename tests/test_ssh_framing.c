#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <liblimeade/liblimeade-internal.h>

static size_t make_frame(unsigned char *out, uint8_t type, const char *body)
{
  struct limeade_packet_flags flags = {0};
  size_t body_size = strlen(body);
  size_t frame_size = sizeof(LIMEADE_MAGIC) + sizeof(flags) + body_size;

  memcpy(out, LIMEADE_MAGIC, sizeof(LIMEADE_MAGIC));
  flags.packet_size = frame_size;
  flags.type = type;
  memcpy(out + sizeof(LIMEADE_MAGIC), &flags, sizeof(flags));
  memcpy(out + sizeof(LIMEADE_MAGIC) + sizeof(flags), body, body_size);
  return frame_size;
}

int main(void)
{
  unsigned char stream[LIMEADE_MAX_PKT_SZ * 2];
  unsigned char first[LIMEADE_MAX_PKT_SZ];
  unsigned char second[LIMEADE_MAX_PKT_SZ];
  size_t first_size = make_frame(first, LIMEADE_PACKET_PERF, "a\xff" "b\xfe");
  size_t second_size = make_frame(second, LIMEADE_PACKET_CLOSE, "c\xff" "d\xfe");
  size_t empty_close_size = make_frame(stream, LIMEADE_PACKET_CLOSE, "\xfe");
  size_t frame_size = 0;

  assert(limeade_ssh_frame_size(first, 1, &frame_size) == 0);
  assert(limeade_ssh_frame_size(first, sizeof(LIMEADE_MAGIC) - 1, &frame_size) == 0);
  assert(limeade_ssh_frame_size(first, sizeof(LIMEADE_MAGIC) + 2, &frame_size) == 0);
  assert(limeade_ssh_frame_size(first, first_size - 1, &frame_size) == 0);
  assert(limeade_ssh_frame_size(first, first_size, &frame_size) == 1);
  assert(frame_size == first_size);
  assert(limeade_ssh_frame_size(stream, empty_close_size, &frame_size) == 1);
  assert(frame_size == empty_close_size);

  memcpy(stream, first, first_size);
  memcpy(stream + first_size, second, second_size);
  assert(limeade_ssh_frame_size(stream, first_size + second_size, &frame_size) == 1);
  assert(frame_size == first_size);
  assert(limeade_ssh_frame_size(stream + frame_size, second_size, &frame_size) == 1);
  assert(frame_size == second_size);

  struct limeade_packet_flags max_flags = {0};
  size_t max_frame_size = UINT16_MAX;
  memset(first, 'x', max_frame_size);
  memcpy(first, LIMEADE_MAGIC, sizeof(LIMEADE_MAGIC));
  max_flags.packet_size = UINT16_MAX;
  max_flags.type = LIMEADE_PACKET_PERF;
  memcpy(first + sizeof(LIMEADE_MAGIC), &max_flags, sizeof(max_flags));
  assert(limeade_ssh_frame_size(first, max_frame_size - 1, &frame_size) == 0);
  assert(limeade_ssh_frame_size(first, max_frame_size, &frame_size) == 1);
  assert(frame_size == max_frame_size);

  memset(stream, 0, sizeof(LIMEADE_MAGIC));
  assert(limeade_ssh_frame_size(stream, sizeof(LIMEADE_MAGIC), &frame_size) == -1);

  make_frame(stream, LIMEADE_PACKET_PERF, "a\xff" "b\xfe");
  ((struct limeade_packet_flags *)(stream + sizeof(LIMEADE_MAGIC)))->packet_size = 1;
  assert(limeade_ssh_frame_size(stream, sizeof(LIMEADE_MAGIC) + sizeof(struct limeade_packet_flags), &frame_size) == -1);

  return 0;
}
