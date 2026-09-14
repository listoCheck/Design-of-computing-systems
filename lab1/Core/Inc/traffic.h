#ifndef TRAFFIC_H
#define TRAFFIC_H
#include <stdbool.h>
#include <stdint.h>
enum { GREEN_MS = 3000, RED_MS = 4 * GREEN_MS, SHORT_RED_MS = RED_MS / 4,
       BLINK_MS = 3000, BLINK_HALF_MS = 500, YELLOW_MS = 1000 };
typedef enum { TRAFFIC_RED, TRAFFIC_GREEN, TRAFFIC_BLINK, TRAFFIC_YELLOW } TrafficState;
typedef enum { LIGHT_OFF, LIGHT_RED, LIGHT_GREEN, LIGHT_YELLOW } Light;
typedef struct { TrafficState state; uint32_t since; bool request; } Traffic;
void traffic_init(Traffic *t, uint32_t now);
void traffic_update(Traffic *t, bool press_event, uint32_t now);
Light traffic_light(const Traffic *t, uint32_t now);
#endif
