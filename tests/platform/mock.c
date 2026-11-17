#include "./mock.h"

gtest_Platform gtest_platform;

bool gplatform_window_open(int width, int height, const char* title, bool vsync, bool resizable, int fps) {
    return gtest_platform.window_open != NULL
        ? gtest_platform.window_open(width, height, title, vsync, resizable, fps)
        : true;
}

void gplatform_window_close(void) {
    if (gtest_platform.window_close != NULL) {
        gtest_platform.window_close();
    }
}

bool gplatform_window_should_close(void) {
    return gtest_platform.window_should_close != NULL ? gtest_platform.window_should_close() : true;
}

void gplatform_begin_drawing(void) {
    if (gtest_platform.begin_drawing != NULL) {
        gtest_platform.begin_drawing();
    }
}

void gplatform_end_drawing(void) {
    if (gtest_platform.end_drawing != NULL) {
        gtest_platform.end_drawing();
    }
}

float gplatform_frame_time(void) {
    return gtest_platform.frame_time != NULL ? gtest_platform.frame_time() : 0;
}

double gplatform_time(void) {
    return gtest_platform.time != NULL ? gtest_platform.time() : 0;
}

int gplatform_width(void) {
    return gtest_platform.width != NULL ? gtest_platform.width() : 0;
}

int gplatform_height(void) {
    return gtest_platform.height != NULL ? gtest_platform.height() : 0;
}

bool gplatform_audio_ready(void) {
    return gtest_platform.audio_ready != NULL ? gtest_platform.audio_ready() : false;
}

void gplatform_audio_open(int buffer_size) {
    if (gtest_platform.audio_open != NULL) {
        gtest_platform.audio_open(buffer_size);
    }
}

void gplatform_audio_close(void) {
    if (gtest_platform.audio_close != NULL) {
        gtest_platform.audio_close();
    }
}

gplatform_Texture gplatform_texture_load(const char* type, const void* bytes, size_t size) {
    return gtest_platform.texture_load != NULL
        ? gtest_platform.texture_load(type, bytes, size)
        : (gplatform_Texture) {0};
}

bool gplatform_texture_valid(gplatform_Texture texture) {
    return gtest_platform.texture_valid != NULL ? gtest_platform.texture_valid(texture) : texture.id != 0;
}

void gplatform_texture_unload(gplatform_Texture texture) {
    if (gtest_platform.texture_unload != NULL) {
        gtest_platform.texture_unload(texture);
    }
}

void gplatform_draw_texture(gplatform_Texture texture, gplatform_TextureDrawParams params) {
    if (gtest_platform.draw_texture != NULL) {
        gtest_platform.draw_texture(texture, params);
    }
}

void gplatform_clear_background(gcolor_Color color) {
    if (gtest_platform.clear_background != NULL) {
        gtest_platform.clear_background(color);
    }
}

bool gplatform_run_loop(gplatform_FrameFn frame, gplatform_LoopEndFn end, void* user) {
    if (gtest_platform.run_loop != NULL) {
        return gtest_platform.run_loop(frame, end, user);
    }

    bool ok = true;
    while (!gplatform_window_should_close()) {
        if (!frame(user, gplatform_frame_time())) {
            ok = false;
            break;
        }
    }
    if (end != NULL) {
        end(user, ok);
    }
    return ok;
}

bool gplatform_loop_running(void) {
    return gtest_platform.loop_running != NULL ? gtest_platform.loop_running() : false;
}

void gplatform_cancel_loop(void* user) {
    if (gtest_platform.cancel_loop != NULL) {
        gtest_platform.cancel_loop(user);
    }
}
