#include <glint/audio.h>

#include <limits.h>
#include <math.h>
#include <raylib.h>

bool gaudio_Device_open(galloc_Allocator alloc, gaudio_Device *device, uint32_t buffer_size) {
    (void)alloc; /* raylib does not expose an allocator for its audio device. */
    if (!device || device->open || buffer_size > INT_MAX) {
        return false;
    }
    if (IsAudioDeviceReady()) {
        *device = (gaudio_Device) {.open = true};
        return true;
    }
    SetAudioStreamBufferSizeDefault(buffer_size ? (int)buffer_size : 2048);
    InitAudioDevice();
    if (!IsAudioDeviceReady()) {
        return false;
    }
    *device = (gaudio_Device) {.open = true, .owns_device = true};
    return true;
}

void gaudio_Device_close(gaudio_Device *device) {
    if (!device) {
        return;
    }
    if (device->open && device->owns_device && IsAudioDeviceReady()) {
        CloseAudioDevice();
    }
    *device = (gaudio_Device) {0};
}

bool gaudio_Device_ready(const gaudio_Device *device) {
    return device && device->open && IsAudioDeviceReady();
}

void gaudio_OnePoleFilter_reset(gaudio_OnePoleFilter *filter) {
    filter->integrator_state = 0.0f;
}

bool gaudio_OnePoleFilter_set_cutoff(gaudio_OnePoleFilter *filter, float cutoff_hz, float sample_rate) {
    if (!filter || !isfinite(cutoff_hz) || !isfinite(sample_rate) || sample_rate <= 0.0f) {
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

float gaudio_OnePoleFilter_process_sample(gaudio_OnePoleFilter *filter, float sample) {
    float result = (filter->integrator_gain * sample + filter->integrator_state) * filter->feedback_solve_gain;
    filter->integrator_state = filter->integrator_gain * (sample - result) + result;
    switch (filter->mode) {
        case GAUDIO_FILTER_LOW_PASS:
            return result;
        case GAUDIO_FILTER_HIGH_PASS:
            return sample - result;
        default:
            return 0.0f;
    }
}

void gaudio_OnePoleFilter_process(gaudio_OnePoleFilter *filter, float *samples, size_t count) {
    for (size_t i = 0; i < count; i++) {
        samples[i] = gaudio_OnePoleFilter_process_sample(filter, samples[i]);
    }
}
