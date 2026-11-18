#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum gdsp_FilterMode {
    GDSP_FILTER_LOW_PASS,
    GDSP_FILTER_HIGH_PASS,
} gdsp_FilterMode;

typedef struct gdsp_OnePoleFilter {
    gdsp_FilterMode mode;
    float integrator_gain;
    float feedback_solve_gain;
    float integrator_state;
} gdsp_OnePoleFilter;

void gdsp_OnePoleFilter_reset(gdsp_OnePoleFilter* filter);

/// Clamp cutoff to [0, 0.497 * sample_rate].
///
/// # Errors
///
/// Nonfinite inputs or nonpositive sample rates return false without changing the filter.
bool gdsp_OnePoleFilter_set_cutoff(gdsp_OnePoleFilter* filter, float cutoff_hz, float sample_rate);

float gdsp_OnePoleFilter_process_sample(gdsp_OnePoleFilter* filter, float sample);

/// Filter samples in place
void gdsp_OnePoleFilter_process(gdsp_OnePoleFilter* filter, float* samples, size_t count);

#ifdef __cplusplus
}
#endif
