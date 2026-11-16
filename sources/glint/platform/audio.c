#include "./audio.h"
#include "./raylib_internal.h"

bool gplatform_audio_ready(void) {
    return IsAudioDeviceReady();
}

void gplatform_audio_open(int buffer_size) {
    graylib_init_logging();

    SetAudioStreamBufferSizeDefault(buffer_size);
    InitAudioDevice();
}

void gplatform_audio_close(void) {
    graylib_init_logging();

    CloseAudioDevice();
}
