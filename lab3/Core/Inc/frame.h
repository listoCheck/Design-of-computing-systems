#ifndef FRAME_H
#define FRAME_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
enum { FRAME_MAX = 192 };
uint16_t frame_crc(const char *text, size_t length);
bool frame_decode(const char *line, char body[FRAME_MAX]);
bool frame_encode(const char *body, char line[FRAME_MAX]);
#endif
