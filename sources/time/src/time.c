#include <glint/time.h>

#include <math.h>
#include <stdint.h>

gtime_Stopwatch gtime_Stopwatch_new(void) {
    return (gtime_Stopwatch) {0};
}

gtime_Stopwatch *gtime_Stopwatch_reset(gtime_Stopwatch *stopwatch) {
    stopwatch->elapsed = 0.0f;
    return stopwatch;
}

gtime_Stopwatch *gtime_Stopwatch_set_elapsed(gtime_Stopwatch *stopwatch, float elapsed) {
    stopwatch->elapsed = elapsed;
    return stopwatch;
}

gtime_Stopwatch *gtime_Stopwatch_tick(gtime_Stopwatch *stopwatch, float dt) {
    if (!stopwatch->is_paused) {
        stopwatch->elapsed += dt;
    }
    return stopwatch;
}

gtime_Stopwatch *gtime_Stopwatch_pause(gtime_Stopwatch *stopwatch) {
    stopwatch->is_paused = true;
    return stopwatch;
}

gtime_Stopwatch *gtime_Stopwatch_unpause(gtime_Stopwatch *stopwatch) {
    stopwatch->is_paused = false;
    return stopwatch;
}

gtime_Timer gtime_Timer_new(float duration, gtime_TimerMode mode) {
    return (gtime_Timer) {.duration = duration, .mode = mode};
}

float gtime_Timer_remaining(const gtime_Timer *timer) {
    return timer->duration - timer->stopwatch.elapsed;
}

float gtime_Timer_fraction(const gtime_Timer *timer) {
    return timer->duration == 0.0f ? 0.0f : timer->stopwatch.elapsed / timer->duration;
}

bool gtime_Timer_just_finished(const gtime_Timer *timer) {
    return timer->times_finished_this_tick > 0;
}

bool gtime_Timer_is_paused(const gtime_Timer *timer) {
    return timer->stopwatch.is_paused;
}

gtime_Timer *gtime_Timer_reset(gtime_Timer *timer) {
    gtime_Stopwatch_reset(&timer->stopwatch);
    timer->is_finished = false;
    timer->times_finished_this_tick = 0;
    return timer;
}

gtime_Timer *gtime_Timer_set_duration(gtime_Timer *timer, float duration) {
    timer->duration = duration;
    return timer;
}

gtime_Timer *gtime_Timer_set_mode(gtime_Timer *timer, gtime_TimerMode mode) {
    timer->mode = mode;
    return timer;
}

gtime_Timer *gtime_Timer_set_elapsed(gtime_Timer *timer, float elapsed) {
    gtime_Stopwatch_set_elapsed(&timer->stopwatch, elapsed);
    return timer;
}

gtime_Timer *gtime_Timer_pause(gtime_Timer *timer) {
    gtime_Stopwatch_pause(&timer->stopwatch);
    return timer;
}

gtime_Timer *gtime_Timer_unpause(gtime_Timer *timer) {
    gtime_Stopwatch_unpause(&timer->stopwatch);
    return timer;
}

gtime_Timer *gtime_Timer_finish(gtime_Timer *timer) {
    return gtime_Timer_tick(timer, gtime_Timer_remaining(timer));
}

gtime_Timer *gtime_Timer_almost_finish(gtime_Timer *timer) {
    return gtime_Timer_tick(timer, nextafterf(gtime_Timer_remaining(timer), -INFINITY));
}

gtime_Timer *gtime_Timer_tick(gtime_Timer *timer, float dt) {
    if (gtime_Timer_is_paused(timer)) {
        timer->times_finished_this_tick = 0;
        if (timer->mode == GTIME_TIMER_REPEATING) {
            timer->is_finished = false;
        }
        return timer;
    }

    if (timer->mode != GTIME_TIMER_REPEATING && timer->is_finished) {
        timer->times_finished_this_tick = 0;
        return timer;
    }

    gtime_Stopwatch_tick(&timer->stopwatch, dt);
    timer->is_finished = timer->stopwatch.elapsed >= timer->duration;
    if (!timer->is_finished) {
        timer->times_finished_this_tick = 0;
    } else if (timer->mode != GTIME_TIMER_REPEATING) {
        timer->times_finished_this_tick = 1;
        timer->stopwatch.elapsed = timer->duration;
    } else if (timer->duration == 0.0f) {
        timer->times_finished_this_tick = UINT32_MAX;
        timer->stopwatch.elapsed = 0.0f;
    } else {
        /* remainderf matches the original timer's elapsed-time behavior. */
        float completions = timer->stopwatch.elapsed / timer->duration;
        if (completions >= 4294967296.0f) {
            timer->times_finished_this_tick = UINT32_MAX;
        } else if (!(completions > 0.0f)) {
            timer->times_finished_this_tick = 0;
        } else {
            timer->times_finished_this_tick = (uint32_t)completions;
        }
        timer->stopwatch.elapsed = remainderf(timer->stopwatch.elapsed, timer->duration);
    }
    return timer;
}
