#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// NOLINTNEXTLINE(cppcoreguidelines-use-enum-class): this is C header
enum GlintAppResult {
    GLINT_APP_CONTINUE,
    GLINT_APP_CLOSE,
    GLINT_APP_ERROR,
};

auto glint_init(void** app, int argc, char** argv) -> GlintAppResult;
auto glint_frame(void*) -> GlintAppResult;
auto glint_close(void*) -> GlintAppResult;

#ifdef __cplusplus
}
#endif
