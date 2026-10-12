#ifndef CLOCK_APP_H
#define CLOCK_APP_H
#include "rtc_device.h"
#include "frame.h"
#include "queue.h"
enum { APP_OUTPUT_SIZE = 2048 };
typedef struct {
    RtcDevice rtc;
    ByteQueue output;
    uint8_t storage[APP_OUTPUT_SIZE];
    char input[FRAME_MAX];
    size_t length;
    bool collecting, oled_ok;
    uint16_t set_id;
    uint32_t received_at, set_polled_at;
} ClockApp;
void app_init(ClockApp *app, uint32_t now);
bool app_can_receive(const ClockApp *app);
void app_receive(ClockApp *app, uint8_t byte, uint32_t now);
void app_service(ClockApp *app, uint32_t now, bool sample_due);
void app_publish(ClockApp *app, uint16_t id);
#endif
