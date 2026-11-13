#ifndef GAUDIO_AUDIO_H_
#define GAUDIO_AUDIO_H_

#include <glint/alloc.h>
#include <glint/modules.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* raylib owns device allocations. A zero-initialized handle that opens an
 * existing device borrows it; closing that handle does not close the device.
 * Any owner must outlive handles borrowing the device. */
typedef struct gaudio_Device {
    bool open;
    bool owns_device;
} gaudio_Device;

/* buffer_size is in sample frames; zero uses the original default (2048).
 * The size takes effect only when this handle opens the device. raylib owns
 * the device's internal allocations and cannot use the supplied allocator. */
bool gaudio_Device_open(galloc_Allocator alloc, gaudio_Device *device, uint32_t buffer_size);
void gaudio_Device_close(gaudio_Device *device);
bool gaudio_Device_ready(const gaudio_Device *device);

/* Borrowed world service. Do not close the device through this pointer. The
 * service exists only while its host or the audio module is loaded. */
typedef struct gaudio_Service {
    gaudio_Device device;
    bool module_owned;
} gaudio_Service;

/* A self-contained descriptor. In a standalone manager, load opens the device
 * and unload closes it. If the host already supplies gaudio_Service, load
 * borrows it instead (including an unavailable/headless device). */
gmod_Module gaudio_module(void);

typedef enum gaudio_FilterMode {
    GAUDIO_FILTER_LOW_PASS,
    GAUDIO_FILTER_HIGH_PASS,
} gaudio_FilterMode;

typedef struct gaudio_OnePoleFilter {
    gaudio_FilterMode mode;
    float integrator_gain;
    float feedback_solve_gain;
    float integrator_state;
} gaudio_OnePoleFilter;

void gaudio_OnePoleFilter_reset(gaudio_OnePoleFilter *filter);
/* Negative cutoffs clamp to zero, upper cutoffs to 0.497 of sample rate.
 * Nonfinite values and nonpositive sample rates are rejected unchanged. */
bool gaudio_OnePoleFilter_set_cutoff(gaudio_OnePoleFilter *filter, float cutoff_hz, float sample_rate);
float gaudio_OnePoleFilter_process_sample(gaudio_OnePoleFilter *filter, float sample);
void gaudio_OnePoleFilter_process(gaudio_OnePoleFilter *filter, float *samples, size_t count);

#ifdef __cplusplus
}
#endif
#endif // GAUDIO_AUDIO_H_
