#ifndef RTC_DEVICE_H
#define RTC_DEVICE_H
#include <stdbool.h>
#include <stdint.h>
typedef struct { uint16_t year; uint8_t month, day, hour, minute, second, weekday; } DateTime;
typedef enum { RTC_STOPPED, RTC_RUNNING, RTC_STARTING, RTC_ERROR } ClockStatus;
enum { RTC_ERR_BUS = 1, RTC_ERR_DATA = 2, RTC_ERR_OSC = 4, RTC_ERR_POWER = 8, RTC_ERR_WRITE = 16 };
typedef struct { DateTime time; ClockStatus status; bool valid; uint32_t errors; } ClockSnapshot;
typedef struct {
    ClockSnapshot snapshot;
    DateTime target;
    bool setting, grace, write_error;
    uint32_t since, start_since;
} RtcDevice;
bool datetime_valid(const DateTime *t);
bool rtc_decode(const uint8_t data[7], DateTime *t);
void rtc_encode(const DateTime *t, uint8_t data[7]);
void rtc_init(RtcDevice *rtc, uint32_t now);
void rtc_sample(RtcDevice *rtc, uint32_t now);
bool rtc_set_begin(RtcDevice *rtc, const DateTime *t, uint32_t now);
/* 0 pending/no job, 1 completed, -1 failed. Wait for oscillator stop without delay. */
int rtc_set_service(RtcDevice *rtc, uint32_t now);
const char *rtc_status_name(ClockStatus status);
#endif
