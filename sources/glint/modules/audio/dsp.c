#include "./dsp.h"

#include <math.h>

void gdsp_OnePoleFilter_reset(gdsp_OnePoleFilter* filter) {
    filter->integrator_state = 0.0f;
}

bool gdsp_OnePoleFilter_set_cutoff(gdsp_OnePoleFilter* filter, float cutoff_hz, float sample_rate) {
    bool frequencies_finite = isfinite(cutoff_hz) != 0 && isfinite(sample_rate) != 0;
    if (filter == NULL || !frequencies_finite || sample_rate <= 0.0f) {
        return false;
    }

    float cutoff = cutoff_hz / sample_rate;
    if (cutoff < 0.0f) {
        cutoff = 0.0f;
    } else if (cutoff > 0.497f) {
        cutoff = 0.497f;
    }

    filter->integrator_gain = tanf(3.14159265358979323846f * cutoff);
    filter->feedback_solve_gain = 1.0f / (1.0f + filter->integrator_gain);
    return true;
}

float gdsp_OnePoleFilter_process_sample(gdsp_OnePoleFilter* filter, float sample) {
    float result = (filter->integrator_gain * sample + filter->integrator_state) * filter->feedback_solve_gain;
    filter->integrator_state = filter->integrator_gain * (sample - result) + result;
    switch (filter->mode) {
        case GDSP_FILTER_LOW_PASS:
            return result;
        case GDSP_FILTER_HIGH_PASS:
            return sample - result;
        default:
            return 0.0f;
    }
}

void gdsp_OnePoleFilter_process(gdsp_OnePoleFilter* filter, float* samples, size_t count) {
    for (size_t i = 0; i < count; i++) {
        samples[i] = gdsp_OnePoleFilter_process_sample(filter, samples[i]);
    }
}
