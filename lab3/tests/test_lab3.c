#include "rtc_device.h"
#include "frame.h"
#include "clock_app.h"
#include "oled.h"
#include "bus.h"
#include <string.h>

volatile unsigned test_failure, test_checks, test_done;
volatile unsigned test_crc_value;
static uint8_t rtc_regs[8];
static unsigned writes, oled_pages;
static bool fail_bus;
bool bus_init(void) { return true; }
bool bus_read(uint8_t address, uint8_t reg, uint8_t *data, uint16_t length)
{
    if (fail_bus || address != 0x6FU || (unsigned)reg + length > sizeof rtc_regs) return false;
    memcpy(data, rtc_regs + reg, length); return true;
}
bool bus_write(uint8_t address, uint8_t reg, const uint8_t *data, uint16_t length)
{
    if (fail_bus) return false;
    if (address == 0x6FU && (unsigned)reg + length <= sizeof rtc_regs) {
        memcpy(rtc_regs + reg, data, length); ++writes; return true;
    }
    if (address == 0x3CU) { ++oled_pages; return true; }
    return false;
}
static void check(bool value, unsigned line) { ++test_checks; if (!value && !test_failure) test_failure = line; }
#define CHECK(x) check((x), __LINE__)
static void load_running(void)
{
    DateTime t = {2026, 10, 12, 14, 5, 9, 1};
    rtc_encode(&t, rtc_regs); rtc_regs[0] |= 0x80U; rtc_regs[3] |= 0x20U; rtc_regs[7] = 0x08U;
}
void test_entry(void)
{
    DateTime leap = {2024, 2, 29, 23, 59, 59, 7};
    DateTime bad = {2025, 2, 29, 0, 0, 0, 1};
    uint8_t bytes[7]; DateTime decoded = {0}; char frame[FRAME_MAX], body[FRAME_MAX];
    CHECK(datetime_valid(&leap)); CHECK(!datetime_valid(&bad));
    rtc_encode(&leap, bytes); CHECK(rtc_decode(bytes, &decoded)); CHECK(decoded.year == 2024 && decoded.day == 29);
    test_crc_value = frame_crc("123456789", 9); CHECK(test_crc_value == 0x29B1U);
    CHECK(frame_encode("7|PING", frame)); frame[strlen(frame) - 1U] = 0; CHECK(frame_decode(frame, body));
    frame[2] = 'X'; CHECK(!frame_decode(frame, body));

    load_running(); RtcDevice rtc; rtc_init(&rtc, 0); CHECK(rtc.snapshot.valid); CHECK(rtc.snapshot.status == RTC_RUNNING);
    DateTime target = {2026, 12, 31, 23, 58, 57, 4};
    CHECK(rtc_set_begin(&rtc, &target, 10)); CHECK(rtc.setting); CHECK((rtc_regs[0] & 0x80U) == 0U); CHECK((rtc_regs[7] & 0x08U) == 0U);
    rtc_regs[3] &= (uint8_t)~0x20U; CHECK(rtc_set_service(&rtc, 30) == 1); CHECK(!rtc.setting); CHECK((rtc_regs[0] & 0x80U) != 0U);
    fail_bus = true; rtc_sample(&rtc, 50); CHECK(rtc.snapshot.status == RTC_ERROR && (rtc.snapshot.errors & RTC_ERR_BUS)); fail_bus = false;

    Oled oled = {0}; CHECK(oled_init(&oled)); oled_render(&oled, &rtc.snapshot); CHECK(oled.page == 0); while (oled.page < 8) CHECK(oled_service(&oled)); CHECK(oled_pages >= 17);
    Canvas c; gfx_clear(&c); gfx_pixel(&c, -1, -1, true); gfx_pixel(&c, 127, 63, true); CHECK(c.bytes[1023] == 0x80U); gfx_line(&c, 0, 0, 127, 63, true); CHECK(c.bytes[0] != 0);

    load_running(); ClockApp app; app_init(&app, 100); CHECK(app_can_receive(&app)); CHECK(frame_encode("1|PING", frame));
    for (char *p = frame; *p; ++p) app_receive(&app, (uint8_t)*p, 101);
    char out[FRAME_MAX] = {0}; unsigned n = 0; uint8_t ch;
    while (queue_pop(&app.output, &ch) && n + 1 < sizeof out) out[n++] = (char)ch;
    if (n && out[n - 1U] == '\n') out[n - 1U] = 0;
    CHECK(frame_decode(out, body)); CHECK(strcmp(body, "1|PONG|1") == 0);
    CHECK(frame_encode("2|SET|2026-10-12|12:34:56|1", frame));
    for (char *p = frame; *p; ++p) app_receive(&app, (uint8_t)*p, 200);
    rtc_regs[3] &= (uint8_t)~0x20U; app_service(&app, 220, false); CHECK(!app.rtc.setting);
    test_done = 1;
}
