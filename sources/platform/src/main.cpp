#include <glint/platform/main.h>

#include <exception>

#include <raylib.h>

#include <glint/config.h>
#include <glint/platform/log.hpp>

#if GLINT_PLATFORM_DESKTOP

auto main(int argc, char** argv) -> int {
    auto app = static_cast<void*>(nullptr);
    auto result = GlintAppResult {};
    auto loop_error = false;

    try {
        result = glint_init(&app, argc, argv);
    } catch (const std::exception& e) {
        glint::log_error("Exception occured while initializing the app: {}", e.what());
        return 1;
    }
    if (result == GLINT_APP_CLOSE) return 0;
    if (result == GLINT_APP_ERROR) return 1;

    try {
        while (true) {
            ::BeginDrawing();
            result = glint_frame(app);
            ::EndDrawing();
            if (result == GLINT_APP_ERROR) loop_error = true;
            if (::WindowShouldClose() or result != GLINT_APP_CONTINUE) break;
        }
    } catch (const std::exception& e) {
        glint::log_error("Exception occured while iterating the app: {}", e.what());
    }

    try {
        result = glint_close(app);
    } catch (const std::exception& e) {
        glint::log_error("Exception occured while closing the app: {}", e.what());
        return 1;
    }
    if (result == GLINT_APP_CLOSE) return loop_error ? 1 : 0;
    if (result == GLINT_APP_ERROR) return 1;

    return 0;
}

#elif defined(GLINT_PLATFORM_WEB)

#include <emscripten.h>

auto glint_emscripten_callback(void* arg) -> void {
    BeginDrawing();
    // TODO: Do we have to catch exception here?
    glint_frame(arg);
    EndDrawing();
}

auto main(int argc, char** argv) -> int {
    auto app = static_cast<void*>(nullptr);

    try {
        glint_init(&app, argc, argv);
    } catch (const std::exception& e) {
        glint::log_error("Exception occured while initializing the app: {}", e.what());
        return 1;
    }

    emscripten_set_main_loop_arg(glint_emscripten_callback, app, 0, false);

    return 0;
}

#else

#error "Current platform is not supported by Glint"

#endif
