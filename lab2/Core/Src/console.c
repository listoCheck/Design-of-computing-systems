#include "console.h"
#include <stdio.h>
#include <string.h>
static void emit(Console *c, const char *text)
{ while (*text) (void)queue_push(&c->output, (uint8_t)*text++); }
void console_init(Console *c, uint32_t now, bool interrupts)
{
    *c = (Console){.interrupts = interrupts};
    traffic_init(&c->traffic, now);
    queue_init(&c->output, c->output_storage, sizeof c->output_storage);
    emit(c, "\r\nUART traffic light, 57600 8N1\r\n");
}
bool console_can_receive(const Console *c)
{ return !c->mode_pending && c->output.capacity - c->output.count >= CONSOLE_RESERVE; }
static bool parse_seconds(const char *text, uint32_t *value)
{
    if (!*text) return false;
    uint32_t n = 0;
    for (; *text; ++text) {
        if (*text < '0' || *text > '9') return false;
        uint32_t digit = (uint32_t)(*text - '0');
        if (n > (2147483U - digit) / 10U) return false;
        n = n * 10U + digit;
    }
    *value = n; return n != 0;
}
static void execute(Console *c)
{
    char response[128];
    uint32_t seconds;
    if (c->too_long) { emit(c, "ERROR: command too long"); return; }
    if (!strcmp(c->line, "?")) {
        (void)snprintf(response, sizeof response, "%s, mode %u, timeout %lu, %c",
                       traffic_name(&c->traffic), c->traffic.button_enabled ? 1U : 2U,
                       (unsigned long)c->traffic.timeout_s, c->interrupts ? 'I' : 'P');
        emit(c, response);
    } else if (!strcmp(c->line, "set mode 1")) {
        traffic_mode(&c->traffic, true); emit(c, "OK");
    } else if (!strcmp(c->line, "set mode 2")) {
        traffic_mode(&c->traffic, false); emit(c, "OK");
    } else if (!strncmp(c->line, "set timeout ", 12)) {
        if (parse_seconds(c->line + 12, &seconds) && traffic_timeout(&c->traffic, seconds)) emit(c, "OK");
        else emit(c, "ERROR: timeout must be 1..2147483 seconds");
    } else if (!strcmp(c->line, "set interrupts on") || !strcmp(c->line, "set interrupts off")) {
        c->requested_interrupts = !strcmp(c->line, "set interrupts on");
        c->mode_pending = c->requested_interrupts != c->interrupts;
        emit(c, "OK");
    } else emit(c, "ERROR: unsupported command");
}
void console_receive(Console *c, uint8_t byte)
{
    (void)queue_push(&c->output, byte); /* echo EVERY received character */
    if (byte == '\n' && c->after_cr) { c->after_cr = false; return; }
    c->after_cr = byte == '\r';
    if (byte == '\r' || byte == '\n') {
        c->line[c->length] = '\0';
        emit(c, "\r\n");
        execute(c);
        emit(c, "\r\n");
        c->length = 0; c->too_long = false;
    } else if (byte == 8 || byte == 127) {
        if (!c->too_long && c->length) --c->length;
    } else if (byte < 32 || byte > 126) {
        c->too_long = true; /* reject embedded NUL / non-ASCII instead of executing a prefix */
    } else if (c->length + 1U < sizeof c->line && !c->too_long) {
        c->line[c->length++] = (char)byte;
    } else c->too_long = true;
}
