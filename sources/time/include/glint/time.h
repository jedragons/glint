#ifndef GTIME_TIME_H
#define GTIME_TIME_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gtime_Stopwatch {
    float elapsed;
    bool is_paused;
} gtime_Stopwatch;

typedef enum gtime_TimerMode {
    GTIME_TIMER_ONCE,
    GTIME_TIMER_REPEATING,
} gtime_TimerMode;

typedef struct gtime_Timer {
    gtime_Stopwatch stopwatch;
    float duration;
    uint32_t times_finished_this_tick;
    gtime_TimerMode mode;
    bool is_finished;
} gtime_Timer;

gtime_Stopwatch gtime_Stopwatch_new(void);
gtime_Stopwatch *gtime_Stopwatch_reset(gtime_Stopwatch *stopwatch);
gtime_Stopwatch *gtime_Stopwatch_set_elapsed(gtime_Stopwatch *stopwatch, float elapsed);
gtime_Stopwatch *gtime_Stopwatch_tick(gtime_Stopwatch *stopwatch, float dt);
gtime_Stopwatch *gtime_Stopwatch_pause(gtime_Stopwatch *stopwatch);
gtime_Stopwatch *gtime_Stopwatch_unpause(gtime_Stopwatch *stopwatch);

gtime_Timer gtime_Timer_new(float duration, gtime_TimerMode mode);
float gtime_Timer_remaining(const gtime_Timer *timer);
float gtime_Timer_fraction(const gtime_Timer *timer);
bool gtime_Timer_just_finished(const gtime_Timer *timer);
bool gtime_Timer_is_paused(const gtime_Timer *timer);
gtime_Timer *gtime_Timer_reset(gtime_Timer *timer);
gtime_Timer *gtime_Timer_set_duration(gtime_Timer *timer, float duration);
gtime_Timer *gtime_Timer_set_mode(gtime_Timer *timer, gtime_TimerMode mode);
gtime_Timer *gtime_Timer_set_elapsed(gtime_Timer *timer, float elapsed);
gtime_Timer *gtime_Timer_pause(gtime_Timer *timer);
gtime_Timer *gtime_Timer_unpause(gtime_Timer *timer);
gtime_Timer *gtime_Timer_finish(gtime_Timer *timer);
gtime_Timer *gtime_Timer_almost_finish(gtime_Timer *timer);
gtime_Timer *gtime_Timer_tick(gtime_Timer *timer, float dt);

#ifdef __cplusplus
}
#endif
#endif
