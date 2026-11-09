#include "frame_stats.h"
#include <limits.h>
void frame_stats_init(FrameStats *s, uint32_t now)
{
    *s = (FrameStats){0};
    s->window_start = now;
    s->fps_min = UINT_MAX;
}
void frame_stats_complete(FrameStats *s, uint32_t now, uint32_t elapsed)
{
    if (s->have_last) {
        uint32_t gap = now - s->last_at;
        if (gap > s->max_gap_ms) s->max_gap_ms = gap;
        if (gap > 50U) ++s->missed;
    }
    s->have_last = 1;
    s->last_at = now;
    if (elapsed > s->max_transfer_ms) s->max_transfer_ms = elapsed;
    ++s->completed;
    s->counter = (uint8_t)((s->counter + 1U) % 100U);
    ++s->window_frames;
    uint32_t window = now - s->window_start;
    if (window >= 1000U) {
        s->fps_last = (uint32_t)(((uint64_t)s->window_frames * 1000U) / window);
        if (s->fps_last < s->fps_min) s->fps_min = s->fps_last;
        s->window_start = now;
        s->window_frames = 0;
    }
}
void frame_stats_fail(FrameStats *s)
{
    ++s->failed;
    s->have_last = 0;
    s->fps_last = 0;
    s->fps_min = 0;
    s->window_start = s->last_at;
    s->window_frames = 0;
}
