#include "frame.h"
#include <stdio.h>
#include <string.h>
uint16_t frame_crc(const char *text, size_t length)
{
    uint16_t crc = 0xFFFFU;
    for (size_t i = 0; i < length; ++i) {
        crc ^= (uint16_t)((uint8_t)text[i] << 8);
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (uint16_t)((crc & 0x8000U)
                                 ? (((uint32_t)crc << 1U) ^ 0x1021U)
                                 : ((uint32_t)crc << 1U));
    }
    return crc;
}
static int hex(char c)
{ return c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }
bool frame_decode(const char *line, char body[FRAME_MAX])
{
    size_t n = strlen(line);
    if (n < 8 || n >= FRAME_MAX || line[0] != '$' || line[n - 5U] != '*') return false;
    uint16_t expected = 0;
    for (size_t i = n - 4U; i < n; ++i) {
        int digit = hex(line[i]); if (digit < 0) return false;
        expected = (uint16_t)((expected << 4) | (unsigned)digit);
    }
    for (size_t i = 1; i < n - 5U; ++i)
        if (line[i] < 32 || line[i] > 126 || line[i] == '$' || line[i] == '*') return false;
    if (frame_crc(line + 1, n - 6U) != expected) return false;
    memcpy(body, line + 1, n - 6U); body[n - 6U] = 0; return true;
}
bool frame_encode(const char *body, char line[FRAME_MAX])
{
    size_t n = strlen(body); if (n + 8U > FRAME_MAX) return false;
    (void)snprintf(line, FRAME_MAX, "$%s*%04X\n", body, frame_crc(body, n)); return true;
}
