#include "rtc_device.h"
#include "bus.h"
#include "board.h"
#include <string.h>
static uint8_t bcd(unsigned n) { return (uint8_t)((n / 10U) * 16U + n % 10U); }
static bool unbcd(uint8_t byte, uint8_t *out)
{
    if ((byte & 15U) > 9U || (byte >> 4) > 9U) return false;
    *out = (uint8_t)((byte >> 4) * 10U + (byte & 15U)); return true;
}
bool datetime_valid(const DateTime *t)
{
    static const uint8_t days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (!t || t->year < 2000 || t->year > 2099 || !t->month || t->month > 12 ||
        t->hour > 23 || t->minute > 59 || t->second > 59 || !t->weekday || t->weekday > 7) return false;
    unsigned max = days[t->month - 1U] + (t->month == 2 && t->year % 4U == 0);
    return t->day && t->day <= max;
}
bool rtc_decode(const uint8_t data[7], DateTime *t)
{
    DateTime v = {0}; uint8_t year;
    if (!unbcd(data[0] & 0x7FU, &v.second) || !unbcd(data[1] & 0x7FU, &v.minute) ||
        !unbcd(data[2] & ((data[2] & 0x40U) ? 0x1FU : 0x3FU), &v.hour) ||
        !unbcd(data[4] & 0x3FU, &v.day) || !unbcd(data[5] & 0x1FU, &v.month) ||
        !unbcd(data[6], &year)) return false;
    if (data[2] & 0x40U) {
        if (!v.hour || v.hour > 12) return false;
        v.hour = (uint8_t)(v.hour % 12U + ((data[2] & 0x20U) ? 12U : 0U));
    }
    v.weekday = data[3] & 7U; v.year = (uint16_t)(2000U + year);
    if (!datetime_valid(&v)) return false;
    *t = v; return true;
}
void rtc_encode(const DateTime *t, uint8_t data[7])
{
    data[0] = bcd(t->second); data[1] = bcd(t->minute); data[2] = bcd(t->hour);
    data[3] = (uint8_t)(t->weekday | 0x08U); /* VBATEN; explicitly acknowledge old PWRFAIL */
    data[4] = bcd(t->day); data[5] = bcd(t->month); data[6] = bcd(t->year - 2000U);
}
const char *rtc_status_name(ClockStatus status)
{
    switch (status) {
    case RTC_STOPPED: return "STOPPED";
    case RTC_RUNNING: return "RUNNING";
    case RTC_STARTING: return "STARTING";
    default: return "ERROR";
    }
}
void rtc_sample(RtcDevice *rtc, uint32_t now)
{
    uint8_t raw[7], second; bool consistent = false;
    uint32_t errors = rtc->write_error ? RTC_ERR_WRITE : 0U;
    for (unsigned tries = 0; tries < 3; ++tries) {
        if (!bus_read(BOARD_RTC_ADDRESS, 0, raw, 7) || !bus_read(BOARD_RTC_ADDRESS, 0, &second, 1)) {
            errors |= RTC_ERR_BUS; break;
        }
        if ((raw[0] & 0x7FU) == (second & 0x7FU)) { consistent = true; break; }
    }
    rtc->snapshot.valid = false;
    if (!consistent) errors |= (errors & RTC_ERR_BUS) ? 0U : RTC_ERR_DATA;
    else {
        rtc->snapshot.valid = rtc_decode(raw, &rtc->snapshot.time);
        if (!rtc->snapshot.valid) errors |= RTC_ERR_DATA;
        if (raw[3] & 0x10U) errors |= RTC_ERR_POWER;
        if ((raw[0] & 0x80U) && !(raw[3] & 0x20U) &&
            !(rtc->grace && (uint32_t)(now - rtc->start_since) < 2000U)) errors |= RTC_ERR_OSC;
    }
    rtc->snapshot.errors = errors;
    if (errors) rtc->snapshot.status = RTC_ERROR;
    else if (!(raw[0] & 0x80U)) rtc->snapshot.status = RTC_STOPPED;
    else rtc->snapshot.status = (raw[3] & 0x20U) ? RTC_RUNNING : RTC_STARTING;
}
void rtc_init(RtcDevice *rtc, uint32_t now)
{ memset(rtc, 0, sizeof *rtc); rtc_sample(rtc, now); }
bool rtc_set_begin(RtcDevice *rtc, const DateTime *t, uint32_t now)
{
    if (rtc->setting || !datetime_valid(t)) return false;
    uint8_t raw[8];
    if (!bus_read(BOARD_RTC_ADDRESS, 0, raw, 8)) return false;
    uint8_t control = raw[7] & (uint8_t)~0x08U;
    uint8_t seconds = raw[0] & 0x7FU;
    if (!bus_write(BOARD_RTC_ADDRESS, 7, &control, 1) || !bus_write(BOARD_RTC_ADDRESS, 0, &seconds, 1)) {
        rtc->write_error = true; return false;
    }
    rtc->target = *t; rtc->since = now; rtc->setting = true; return true;
}
int rtc_set_service(RtcDevice *rtc, uint32_t now)
{
    if (!rtc->setting) return 0;
    uint8_t weekday;
    if (!bus_read(BOARD_RTC_ADDRESS, 3, &weekday, 1)) goto failed;
    if (weekday & 0x20U) {
        if ((uint32_t)(now - rtc->since) < 2000U) return 0;
        goto failed;
    }
    uint8_t raw[7], check[7]; rtc_encode(&rtc->target, raw);
    if (!bus_write(BOARD_RTC_ADDRESS, 0, raw, 7)) goto failed;
    /* Verify while stopped, then start. Never confirm an unverified partial write. */
    if (!bus_read(BOARD_RTC_ADDRESS, 0, check, 7)) goto failed;
    DateTime actual;
    if (!rtc_decode(check, &actual) || actual.year != rtc->target.year || actual.month != rtc->target.month ||
        actual.day != rtc->target.day || actual.hour != rtc->target.hour || actual.minute != rtc->target.minute ||
        actual.second != rtc->target.second || actual.weekday != rtc->target.weekday) goto failed;
    raw[0] |= 0x80U;
    if (!bus_write(BOARD_RTC_ADDRESS, 0, raw, 1)) goto failed;
    rtc->setting = false; rtc->write_error = false;
    rtc->grace = true; rtc->start_since = now; rtc_sample(rtc, now); return 1;
failed:
    rtc->setting = false; rtc->write_error = true; rtc_sample(rtc, now); return -1;
}
