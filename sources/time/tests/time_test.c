#include <glint/time.h>

#include <mewunit.h>
#include <math.h>
#include <stdint.h>

TEST(stopwatch, {
    gtime_Stopwatch stopwatch = gtime_Stopwatch_new();
    mewassert("Unexpected timer state", stopwatch.elapsed == 0.0f && !stopwatch.is_paused);
    gtime_Stopwatch_tick(&stopwatch, 0.5f);
    gtime_Stopwatch_pause(&stopwatch);
    gtime_Stopwatch_tick(&stopwatch, 3.0f);
    mewassert("Unexpected timer state", stopwatch.elapsed == 0.5f && stopwatch.is_paused);
    gtime_Stopwatch_reset(&stopwatch);
    mewassert("Unexpected timer state", stopwatch.elapsed == 0.0f && stopwatch.is_paused);
    gtime_Stopwatch_unpause(&stopwatch);
    gtime_Stopwatch_set_elapsed(&stopwatch, 2.0f);
    gtime_Stopwatch_tick(&stopwatch, 0.25f);
    mewassert("Unexpected timer state", stopwatch.elapsed == 2.25f);
})

TEST(once, {
    gtime_Timer timer = gtime_Timer_new(2.0f, GTIME_TIMER_ONCE);
    gtime_Timer_tick(&timer, 0.5f);
    mewassert("Unexpected timer state", timer.stopwatch.elapsed == 0.5f);
    mewassert("Unexpected timer state", gtime_Timer_remaining(&timer) == 1.5f && gtime_Timer_fraction(&timer) == 0.25f);
    gtime_Timer_pause(&timer);
    gtime_Timer_tick(&timer, 2.0f);
    mewassert("Unexpected timer state", !timer.is_finished && !gtime_Timer_just_finished(&timer));
    gtime_Timer_unpause(&timer);
    gtime_Timer_tick(&timer, 3.0f);
    mewassert("Unexpected timer state", timer.is_finished && gtime_Timer_just_finished(&timer));
    mewassert("Unexpected timer state", timer.stopwatch.elapsed == 2.0f && timer.times_finished_this_tick == 1);
    gtime_Timer_tick(&timer, 1.0f);
    mewassert("Unexpected timer state", timer.is_finished && !gtime_Timer_just_finished(&timer));
    gtime_Timer_pause(&timer);
    gtime_Timer_reset(&timer);
    mewassert("Unexpected timer state", !timer.is_finished && gtime_Timer_is_paused(&timer));
    gtime_Timer_unpause(&timer);
    gtime_Timer_finish(&timer);
    mewassert("Unexpected timer state", timer.is_finished);
    gtime_Timer_reset(&timer);
    gtime_Timer_almost_finish(&timer);
    mewassert("Unexpected timer state", !timer.is_finished && timer.stopwatch.elapsed < timer.duration);
    gtime_Timer_set_elapsed(&timer, 1.0f);
    gtime_Timer_set_duration(&timer, 1.0f);
    gtime_Timer_tick(&timer, 0.0f);
    mewassert("Unexpected timer state", timer.is_finished);
})

TEST(repeating, {
    gtime_Timer timer = gtime_Timer_new(2.0f, GTIME_TIMER_REPEATING);
    gtime_Timer_tick(&timer, 4.5f);
    mewassert("Unexpected timer state", timer.is_finished && timer.times_finished_this_tick == 2);
    mewassert("Unexpected timer state", timer.stopwatch.elapsed == 0.5f);
    gtime_Timer_pause(&timer);
    gtime_Timer_tick(&timer, 1.0f);
    mewassert("Unexpected timer state", !timer.is_finished && timer.times_finished_this_tick == 0);
    mewassert("Unexpected timer state", timer.stopwatch.elapsed == 0.5f);
    gtime_Timer_unpause(&timer);
    gtime_Timer_tick(&timer, 1.0f);
    mewassert("Unexpected timer state", !timer.is_finished);
    gtime_Timer_tick(&timer, 0.5f);
    mewassert("Unexpected timer state", timer.is_finished && timer.times_finished_this_tick == 1);
    mewassert("Unexpected timer state", timer.stopwatch.elapsed == 0.0f);
    gtime_Timer_set_duration(&timer, 0.0f);
    gtime_Timer_tick(&timer, 0.1f);
    mewassert("Unexpected timer state", timer.is_finished && timer.times_finished_this_tick == UINT32_MAX);
    mewassert("Unexpected timer state", gtime_Timer_fraction(&timer) == 0.0f);
    gtime_Timer_set_mode(&timer, GTIME_TIMER_ONCE);
    gtime_Timer_reset(&timer);
    gtime_Timer_tick(&timer, 0.0f);
    mewassert("Unexpected timer state", timer.is_finished && timer.times_finished_this_tick == 1);
})

mewtest_main();
