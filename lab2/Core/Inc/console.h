#ifndef CONSOLE_H
#define CONSOLE_H
#include "traffic.h"
#include "queue.h"
enum { CONSOLE_LINE_SIZE = 80, CONSOLE_OUTPUT_SIZE = 512, CONSOLE_RESERVE = 160 };
typedef struct {
    Traffic traffic;
    char line[CONSOLE_LINE_SIZE];
    size_t length;
    bool too_long, after_cr, interrupts, mode_pending, requested_interrupts;
    ByteQueue output;
    uint8_t output_storage[CONSOLE_OUTPUT_SIZE];
} Console;
void console_init(Console *c, uint32_t now, bool interrupts);
bool console_can_receive(const Console *c);
/* Called only when can_receive; produces exact byte echo and CRLF responses. */
void console_receive(Console *c, uint8_t byte);
#endif
