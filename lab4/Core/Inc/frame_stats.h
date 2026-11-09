#ifndef FRAME_STATS_H
#define FRAME_STATS_H
#include <stdint.h>
typedef struct {
    uint32_t completed, failed, missed, last_at, max_gap_ms, max_transfer_ms;
    uint32_t window_start, window_frames, fps_last, fps_min;
    uint8_t counter;
    uint8_t have_last;
} FrameStats;
void frame_stats_init(FrameStats *stats, uint32_t now);
void frame_stats_complete(FrameStats *stats, uint32_t now, uint32_t elapsed);
void frame_stats_fail(FrameStats *stats);
#endif
