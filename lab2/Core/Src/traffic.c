#include "traffic.h"
void traffic_init(Traffic *t, uint32_t now)
{ *t = (Traffic){.state = TRAFFIC_RED, .since = now, .timeout_s = 12, .button_enabled = true}; }
void traffic_mode(Traffic *t, bool enabled)
{ t->button_enabled = enabled; if (!enabled) t->request = false; }
bool traffic_timeout(Traffic *t, uint32_t seconds)
{
    /* Keep intervals below half the uint32 tick range. */
    if (!seconds || seconds > 2147483U) return false;
    t->timeout_s = seconds; return true;
}
void traffic_update(Traffic *t, bool press, uint32_t now)
{
    if (press && t->button_enabled && t->state != TRAFFIC_GREEN) t->request = true;
    uint32_t red = t->timeout_s * 1000U;
    uint32_t duration = t->state == TRAFFIC_RED ? (t->request ? red / 4U : red) :
                        t->state == TRAFFIC_YELLOW ? 1000U : 3000U;
    if ((uint32_t)(now - t->since) < duration) return;
    if (t->state == TRAFFIC_RED) t->request = false;
    t->state = (TrafficState)(((unsigned)t->state + 1U) % 4U);
    t->since = now;
}
uint16_t traffic_leds(const Traffic *t, uint32_t now)
{
    switch (t->state) {
    case TRAFFIC_RED: return 1U << 15;
    case TRAFFIC_YELLOW: return 1U << 14;
    case TRAFFIC_GREEN: return 1U << 13;
    case TRAFFIC_BLINK: return ((uint32_t)(now - t->since) / 500U) % 2U ? 0 : 1U << 13;
    default: return 0;
    }
}
const char *traffic_name(const Traffic *t)
{
    switch (t->state) {
    case TRAFFIC_RED: return "red";
    case TRAFFIC_GREEN: return "green";
    case TRAFFIC_BLINK: return "blinking green";
    case TRAFFIC_YELLOW: return "yellow";
    default: return "unknown";
    }
}
