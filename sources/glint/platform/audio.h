#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool gplatform_audio_ready(void);

void gplatform_audio_open(int buffer_size);

void gplatform_audio_close(void);

#ifdef __cplusplus
}
#endif
