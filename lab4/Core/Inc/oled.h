#ifndef OLED_H
#define OLED_H
#include "gfx.h"
#include "rtc_device.h"
typedef struct { Canvas canvas; unsigned page; bool ready; } Oled;
bool oled_init(Oled *display);
void oled_render(Oled *display, const ClockSnapshot *snapshot, unsigned counter);
bool oled_service(Oled *display);
#endif
