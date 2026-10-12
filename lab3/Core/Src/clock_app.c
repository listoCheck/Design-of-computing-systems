#include "clock_app.h"
#include "uart_driver.h"
#include <stdio.h>
#include <string.h>
static void reply(ClockApp *app, const char *body)
{
    char line[FRAME_MAX];
    if (!frame_encode(body, line) || strlen(line) > app->output.capacity - app->output.count) return;
    for (const char *p = line; *p; ++p) (void)queue_push(&app->output, (uint8_t)*p);
}
static void error(ClockApp *app, uint16_t id, const char *code)
{ char body[FRAME_MAX]; (void)snprintf(body, sizeof body, "%u|ERR|%s", id, code); reply(app, body); }
void app_publish(ClockApp *app, uint16_t id)
{
    char body[FRAME_MAX]; const ClockSnapshot *s = &app->rtc.snapshot; DateTime t = s->time;
    if (!s->valid) t = (DateTime){0};
    UartState u = uart_state();
    (void)snprintf(body, sizeof body, "%u|STATE|%04u-%02u-%02u|%02u:%02u:%02u|%u|%s|%u|%lu|%u|%lu|%lu",
        id, t.year, t.month, t.day, t.hour, t.minute, t.second, t.weekday, rtc_status_name(s->status),
        s->valid ? 1U : 0U, (unsigned long)s->errors, app->oled_ok ? 1U : 0U,
        (unsigned long)u.rx_overflow, (unsigned long)u.hardware_errors);
    reply(app, body);
}
void app_init(ClockApp *app, uint32_t now)
{
    memset(app, 0, sizeof *app); queue_init(&app->output, app->storage, sizeof app->storage);
    rtc_init(&app->rtc, now);
}
bool app_can_receive(const ClockApp *app)
{ return app->output.capacity - app->output.count >= 3U * FRAME_MAX; }
static bool number(const char *text, unsigned max, unsigned *value)
{
    if (!*text) return false;
    unsigned n = 0;
    for (; *text; ++text) {
        if (*text < '0' || *text > '9') return false;
        unsigned d = (unsigned)(*text - '0');
        if (n > max / 10U || (n == max / 10U && d > max % 10U)) return false;
        n = n * 10U + d;
    }
    *value = n; return true;
}
static bool date_time(const char *date, const char *time, const char *weekday, DateTime *t)
{
    if (strlen(date) != 10 || date[4] != '-' || date[7] != '-' || strlen(time) != 8 || time[2] != ':' || time[5] != ':') return false;
    const unsigned starts[] = {0,5,8,0,3,6};
    unsigned values[7];
    for (unsigned i = 0; i < 6; ++i) {
        char field[5] = {0}; unsigned size = i == 0 ? 4U : 2U;
        memcpy(field, (i < 3 ? date : time) + starts[i], size);
        if (!number(field, i == 0 ? 2099U : 99U, &values[i])) return false;
    }
    if (!number(weekday, 7, &values[6])) return false;
    *t = (DateTime){(uint16_t)values[0], (uint8_t)values[1], (uint8_t)values[2], (uint8_t)values[3],
                    (uint8_t)values[4], (uint8_t)values[5], (uint8_t)values[6]};
    return datetime_valid(t);
}
static void execute(ClockApp *app, uint32_t now)
{
    char body[FRAME_MAX];
    if (!frame_decode(app->input, body)) { error(app, 0, "CRC"); return; }
    char *parts[6] = {body}; unsigned count = 1;
    for (char *p = body; *p; ++p) if (*p == '|') {
        if (count == 6) { error(app, 0, "FORMAT"); return; }
        *p = 0; parts[count++] = p + 1;
    }
    unsigned id;
    if (count < 2 || !number(parts[0], 65535, &id) || !id) { error(app, 0, "FORMAT"); return; }
    char response[FRAME_MAX];
    if (count == 2 && !strcmp(parts[1], "PING")) {
        (void)snprintf(response, sizeof response, "%u|PONG|1", id); reply(app, response);
    } else if (count == 2 && !strcmp(parts[1], "GET")) {
        rtc_sample(&app->rtc, now); app_publish(app, (uint16_t)id);
    } else if (count == 5 && !strcmp(parts[1], "SET")) {
        DateTime t;
        if (!date_time(parts[2], parts[3], parts[4], &t)) error(app, (uint16_t)id, "VALUE");
        else if (app->rtc.setting) error(app, (uint16_t)id, "BUSY");
        else if (!rtc_set_begin(&app->rtc, &t, now)) {
            rtc_sample(&app->rtc, now); error(app, (uint16_t)id, "I2C");
        } else { app->set_id = (uint16_t)id; app->set_polled_at = now; }
    } else error(app, (uint16_t)id, "COMMAND");
}
void app_receive(ClockApp *app, uint8_t byte, uint32_t now)
{
    if (byte == '$') { app->length = 0; app->collecting = true; }
    if (!app->collecting) return;
    app->received_at = now;
    if (byte == '\n') {
        app->input[app->length] = 0; app->collecting = false; execute(app, now); return;
    }
    if (byte < 32 || byte > 126 || app->length + 1U >= sizeof app->input) {
        app->collecting = false; error(app, 0, "FRAME"); return;
    }
    app->input[app->length++] = (char)byte;
}
void app_service(ClockApp *app, uint32_t now, bool sample_due)
{
    if (app->collecting && (uint32_t)(now - app->received_at) > 500U) {
        app->collecting = false; error(app, 0, "TIMEOUT");
    }
    if (app->rtc.setting && (uint32_t)(now - app->set_polled_at) >= 20U && app_can_receive(app)) {
        app->set_polled_at = now;
        int result = rtc_set_service(&app->rtc, now);
        if (result == 1) {
            char body[FRAME_MAX]; (void)snprintf(body, sizeof body, "%u|OK|SET", app->set_id);
            reply(app, body); app_publish(app, 0);
        } else if (result < 0) { error(app, app->set_id, "RTC_WRITE"); app_publish(app, 0); }
    }
    if (sample_due) { rtc_sample(&app->rtc, now); if (app_can_receive(app)) app_publish(app, 0); }
}
