#include "oled.h"
#include "bus.h"
#include "board.h"
#include <stdio.h>
bool oled_init(Oled *display)
{
    static const uint8_t commands[] = {0xAE,0xD5,0x80,0xA8,0x3F,0xD3,0,0x40,0x8D,0x14,
        0x20,2,0xA1,0xC8,0xDA,0x12,0x81,0x7F,0xD9,0xF1,0xDB,0x40,0xA4,0xA6,0xAF};
    display->ready = bus_write(BOARD_OLED_ADDRESS, 0, commands, sizeof commands);
    display->page = 8; gfx_clear(&display->canvas); return display->ready;
}
void oled_render(Oled *display, const ClockSnapshot *snapshot)
{
    if (!display->ready || display->page < 8) return; /* frozen buffer until all pages sent */
    Canvas *canvas = &display->canvas; gfx_clear(canvas);
    gfx_text(canvas, 2, 0, "MCP79411", 1);
    char text[32]; (void)snprintf(text, sizeof text, "E:%02lX", (unsigned long)snapshot->errors);
    gfx_text(canvas, 98, 0, text, 1); gfx_line(canvas, 0, 9, 127, 9, true);
    if (snapshot->valid) {
        (void)snprintf(text, sizeof text, "%02u:%02u:%02u", snapshot->time.hour, snapshot->time.minute, snapshot->time.second);
        gfx_text(canvas, 16, 13, text, 2);
        (void)snprintf(text, sizeof text, "%02u/%02u/%02u", snapshot->time.day, snapshot->time.month, snapshot->time.year % 100U);
        gfx_text(canvas, 16, 33, text, 2);
        static const char *const days[] = {"MON","TUE","WED","THU","FRI","SAT","SUN"};
        gfx_text(canvas, 2, 54, days[snapshot->time.weekday - 1U], 1);
    } else {
        gfx_text(canvas, 16, 13, "--:--:--", 2); gfx_text(canvas, 16, 33, "--/--/--", 2);
        gfx_text(canvas, 2, 54, "---", 1);
    }
    gfx_text(canvas, 32, 54, rtc_status_name(snapshot->status), 1);
    display->page = 0;
}
bool oled_service(Oled *display)
{
    if (!display->ready) return false;
    if (display->page >= 8) return true;
    uint8_t command[] = {(uint8_t)(0xB0U + display->page),0,0x10};
    if (!bus_write(BOARD_OLED_ADDRESS, 0, command, sizeof command) ||
        !bus_write(BOARD_OLED_ADDRESS, 0x40, &display->canvas.bytes[display->page * 128U], 128)) {
        display->ready = false; return false;
    }
    ++display->page; return true;
}
