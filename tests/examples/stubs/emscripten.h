#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void emscripten_set_main_loop_arg(void (*callback)(void*), void* arg, int fps, int simulate_infinite_loop);
void emscripten_cancel_main_loop(void);

#ifdef __cplusplus
}
#endif
