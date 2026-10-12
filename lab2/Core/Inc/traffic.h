#ifndef TRAFFIC_H
#define TRAFFIC_H
#include <stdbool.h>
#include <stdint.h>
typedef enum { TRAFFIC_RED, TRAFFIC_GREEN, TRAFFIC_BLINK, TRAFFIC_YELLOW } TrafficState;
typedef struct { TrafficState state; uint32_t since, timeout_s; bool request, button_enabled; } Traffic;
void traffic_init(Traffic *t, uint32_t now);
void traffic_update(Traffic *t, bool press, uint32_t now);
void traffic_mode(Traffic *t, bool enabled);
bool traffic_timeout(Traffic *t, uint32_t seconds);
uint16_t traffic_leds(const Traffic *t, uint32_t now);
const char *traffic_name(const Traffic *t);
#endif
