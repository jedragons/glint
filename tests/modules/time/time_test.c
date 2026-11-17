#include <glint/modules/time/time.h>
#include <glint/unit/unit.h>

#include <math.h>
#include <stdint.h>

GUNIT_TEST(stopwatch_starts_unpaused,
    gtime_Stopwatch stopwatch = gtime_Stopwatch_new();
    gunit_assert("starts at zero", stopwatch.elapsed == 0.0f && !stopwatch.is_paused);
)

GUNIT_TEST(stopwatch_pause_prevents_elapsed_growth,
    gtime_Stopwatch stopwatch = gtime_Stopwatch_new();
    gtime_Stopwatch_tick(&stopwatch, .5f);
    gtime_Stopwatch_pause(&stopwatch);
    gtime_Stopwatch_tick(&stopwatch, 3);
    gunit_assert("paused elapsed unchanged", stopwatch.elapsed == .5f && stopwatch.is_paused);
)

GUNIT_TEST(stopwatch_reset_preserves_pause,
    gtime_Stopwatch stopwatch = gtime_Stopwatch_new();
    gtime_Stopwatch_tick(&stopwatch, .5f);
    gtime_Stopwatch_pause(&stopwatch);
    gtime_Stopwatch_reset(&stopwatch);
    gunit_assert("reset clears elapsed while paused", stopwatch.elapsed == 0 && stopwatch.is_paused);
)

GUNIT_TEST(stopwatch_unpause_and_set_elapsed,
    gtime_Stopwatch stopwatch = gtime_Stopwatch_new();
    gtime_Stopwatch_pause(&stopwatch);
    gtime_Stopwatch_unpause(&stopwatch);
    gtime_Stopwatch_set_elapsed(&stopwatch, 2);
    gtime_Stopwatch_tick(&stopwatch, .25f);
    gunit_assert("unpaused stopwatch advances from set value", stopwatch.elapsed == 2.25f);
)

GUNIT_TEST(once_timer_progress,
    gtime_Timer timer = gtime_Timer_new(2, GTIME_TIMER_ONCE);
    gtime_Timer_tick(&timer, .5f);
    gunit_assert("elapsed follows tick", timer.stopwatch.elapsed == .5f);
    gunit_assert(
        "remaining and fraction reflect progress",
        gtime_Timer_remaining(&timer) == 1.5f && gtime_Timer_fraction(&timer) == .25f
    );
)

GUNIT_TEST(once_timer_pause_prevents_finish,
    gtime_Timer timer = gtime_Timer_new(2, GTIME_TIMER_ONCE);
    gtime_Timer_pause(&timer);
    gtime_Timer_tick(&timer, 3);
    gunit_assert("paused timer cannot finish", !timer.is_finished && !gtime_Timer_just_finished(&timer));
)

GUNIT_TEST(once_timer_finishes_only_once,
    gtime_Timer timer = gtime_Timer_new(2, GTIME_TIMER_ONCE);
    gtime_Timer_pause(&timer);
    gtime_Timer_unpause(&timer);
    gtime_Timer_tick(&timer, 3);
    gunit_assert("finish edge reported", timer.is_finished && gtime_Timer_just_finished(&timer));
    gunit_assert("elapsed clamps at duration", timer.stopwatch.elapsed == 2 && timer.times_finished_this_tick == 1);
    gtime_Timer_tick(&timer, 1);
    gunit_assert(
        "finished state persists without another edge",
        timer.is_finished && !gtime_Timer_just_finished(&timer)
    );
)

GUNIT_TEST(once_timer_reset_preserves_pause,
    gtime_Timer timer = gtime_Timer_new(2, GTIME_TIMER_ONCE);
    gtime_Timer_finish(&timer);
    gtime_Timer_pause(&timer);
    gtime_Timer_reset(&timer);
    gunit_assert(
        "reset clears finished state and preserves pause",
        !timer.is_finished && gtime_Timer_is_paused(&timer)
    );
)

GUNIT_TEST(once_timer_explicit_finish,
    gtime_Timer timer = gtime_Timer_new(2, GTIME_TIMER_ONCE);
    gtime_Timer_finish(&timer);
    gunit_assert("explicit finish marks timer finished", timer.is_finished);
)

GUNIT_TEST(once_timer_almost_finish,
    gtime_Timer timer = gtime_Timer_new(2, GTIME_TIMER_ONCE);
    gtime_Timer_almost_finish(&timer);
    gunit_assert("almost finish stays below duration", !timer.is_finished && timer.stopwatch.elapsed < timer.duration);
)

GUNIT_TEST(once_timer_duration_changes_apply_next_tick,
    gtime_Timer timer = gtime_Timer_new(2, GTIME_TIMER_ONCE);
    gtime_Timer_set_elapsed(&timer, 1);
    gtime_Timer_set_duration(&timer, 1);
    gtime_Timer_tick(&timer, 0);
    gunit_assert("duration change finishes timer", timer.is_finished);
)

GUNIT_TEST(repeating_timer_counts_multiple_completions,
    gtime_Timer timer = gtime_Timer_new(2, GTIME_TIMER_REPEATING);
    gtime_Timer_tick(&timer, 4.5f);
    gunit_assert("two durations completed", timer.is_finished && timer.times_finished_this_tick == 2);
    gunit_assert("elapsed wraps to remainder", timer.stopwatch.elapsed == .5f);
)

GUNIT_TEST(repeating_timer_pause_clears_completion_edge,
    gtime_Timer timer = gtime_Timer_new(2, GTIME_TIMER_REPEATING);
    gtime_Timer_tick(&timer, 4.5f);
    gtime_Timer_pause(&timer);
    gtime_Timer_tick(&timer, 1);
    gunit_assert("paused tick has no completion", !timer.is_finished && timer.times_finished_this_tick == 0);
    gunit_assert("elapsed stays unchanged", timer.stopwatch.elapsed == .5f);
)

GUNIT_TEST(repeating_timer_resumes_to_next_completion,
    gtime_Timer timer = gtime_Timer_new(2, GTIME_TIMER_REPEATING);
    gtime_Timer_tick(&timer, .5f);
    gtime_Timer_pause(&timer);
    gtime_Timer_unpause(&timer);
    gtime_Timer_tick(&timer, 1);
    gunit_assert("partial duration unfinished", !timer.is_finished);
    gtime_Timer_tick(&timer, .5f);
    gunit_assert("exact completion counted", timer.is_finished && timer.times_finished_this_tick == 1);
    gunit_assert("elapsed wraps to zero", timer.stopwatch.elapsed == 0);
)

GUNIT_TEST(repeating_zero_duration_saturates_completion_count,
    gtime_Timer timer = gtime_Timer_new(2, GTIME_TIMER_REPEATING);
    gtime_Timer_set_duration(&timer, 0);
    gtime_Timer_tick(&timer, .1f);
    gunit_assert(
        "zero duration saturates completion count",
        timer.is_finished && timer.times_finished_this_tick == UINT32_MAX
    );
    gunit_assert("zero duration fraction defined", gtime_Timer_fraction(&timer) == 0);
)

GUNIT_TEST(zero_duration_once_mode_finishes_once,
    gtime_Timer timer = gtime_Timer_new(0, GTIME_TIMER_REPEATING);
    gtime_Timer_set_mode(&timer, GTIME_TIMER_ONCE);
    gtime_Timer_reset(&timer);
    gtime_Timer_tick(&timer, 0);
    gunit_assert("zero duration once finishes once", timer.is_finished && timer.times_finished_this_tick == 1);
)

gunit_main();
