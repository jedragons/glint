#include <glint/platform/audio.h>
#include <glint/platform/rendering.h>
#include <glint/platform/windowing.h>

#ifdef GLINT_MOCK_GAME
#include <limits.h>
#include <raylib.h>
#endif

#ifndef GLINT_MOCK_GAME
bool gplatform_window_open(int w, int h, const char* t, bool v, bool r, int fps) {
    (void)w;
    (void)h;
    (void)t;
    (void)v;
    (void)r;
    (void)fps;
    return true;
}

void gplatform_window_close(void) {}

bool gplatform_window_should_close(void) {
    return true;
}

void gplatform_begin_drawing(void) {}

void gplatform_end_drawing(void) {}

#else
bool gplatform_window_open(int w, int h, const char* t, bool v, bool r, int fps) {
    unsigned flags = (v ? FLAG_VSYNC_HINT : 0) | (r ? FLAG_WINDOW_RESIZABLE : 0);
    if (flags > 0) {
        SetConfigFlags(flags);
    }

    InitWindow(w, h, t);
    if (!IsWindowReady()) {
        return false;
    }

    SetTargetFPS(fps);
    return true;
}

void gplatform_window_close(void) {
    CloseWindow();
}

bool gplatform_window_should_close(void) {
    return WindowShouldClose();
}

void gplatform_begin_drawing(void) {
    BeginDrawing();
}

void gplatform_end_drawing(void) {
    EndDrawing();
}

#endif

#ifndef GLINT_MOCK_WINDOW
#ifdef GLINT_MOCK_GAME
float gplatform_frame_time(void) {
    return GetFrameTime();
}

double gplatform_time(void) {
    return GetTime();
}

int gplatform_width(void) {
    return GetScreenWidth();
}

int gplatform_height(void) {
    return GetScreenHeight();
}

#else
float gplatform_frame_time(void) {
    return 0;
}

double gplatform_time(void) {
    return 0;
}

int gplatform_width(void) {
    return 0;
}

int gplatform_height(void) {
    return 0;
}

#endif
#endif

#ifndef GLINT_MOCK_AUDIO
#ifdef GLINT_MOCK_GAME
bool gplatform_audio_ready(void) {
    return IsAudioDeviceReady();
}

void gplatform_audio_open(int n) {
    SetAudioStreamBufferSizeDefault(n);
    InitAudioDevice();
}

void gplatform_audio_close(void) {
    CloseAudioDevice();
}
#else
bool gplatform_audio_ready(void) {
    return false;
}

void gplatform_audio_open(int n) {
    (void)n;
}

void gplatform_audio_close(void) {}
#endif
#endif

#ifndef GLINT_MOCK_RENDER
#ifdef GLINT_MOCK_GAME
gplatform_Texture gplatform_texture_load(const char* type, const void* bytes, size_t size) {
    if (type == NULL || *type == 0 || bytes == NULL || size == 0 || size > INT_MAX) {
        return (gplatform_Texture) {0};
    }

    Image image = LoadImageFromMemory(type, bytes, (int)size);
    if (!IsImageValid(image)) {
        return (gplatform_Texture) {0};
    }

    Texture2D t = LoadTextureFromImage(image);
    UnloadImage(image);
    return (gplatform_Texture) {t.id, t.width, t.height, t.mipmaps, t.format};
}

bool gplatform_texture_valid(gplatform_Texture t) {
    return IsTextureValid((Texture2D) {t.id, t.width, t.height, t.mipmaps, t.format});
}

void gplatform_texture_unload(gplatform_Texture t) {
    UnloadTexture((Texture2D) {t.id, t.width, t.height, t.mipmaps, t.format});
}

void gplatform_draw_texture(gplatform_Texture t, gplatform_TextureDrawParams p) {
    DrawTexturePro(
        (Texture2D) {t.id, t.width, t.height, t.mipmaps, t.format},
        (Rectangle) {p.source.x, p.source.y, p.source.w, p.source.h},
        (Rectangle) {p.dest.x, p.dest.y, p.dest.w, p.dest.h},
        (Vector2) {p.origin.x, p.origin.y},
        p.rotation,
        (Color) {p.tint.r, p.tint.g, p.tint.b, p.tint.a}
    );
}

void gplatform_clear_background(gcolor_Color c) {
    ClearBackground((Color) {c.r, c.g, c.b, c.a});
}
#else
gplatform_Texture gplatform_texture_load(const char* type, const void* bytes, size_t size) {
    (void)type;
    (void)bytes;
    (void)size;
    return (gplatform_Texture) {0};
}

bool gplatform_texture_valid(gplatform_Texture t) {
    return t.id != 0;
}

void gplatform_texture_unload(gplatform_Texture t) {
    (void)t;
}

void gplatform_draw_texture(gplatform_Texture t, gplatform_TextureDrawParams p) {
    (void)t;
    (void)p;
}

void gplatform_clear_background(gcolor_Color c) {
    (void)c;
}
#endif
#endif

bool gplatform_run_loop(gplatform_FrameFn fn, gplatform_LoopEndFn end, void* user) {
    bool ok = true;
    while (!gplatform_window_should_close()) {
        if (!fn(user, gplatform_frame_time())) {
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
    return false;
}

void gplatform_cancel_loop(void* user) {
    (void)user;
}
