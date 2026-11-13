#ifndef GLINT_TEST_EMSCRIPTEN_H
#define GLINT_TEST_EMSCRIPTEN_H
void emscripten_set_main_loop_arg(void (*callback)(void *), void *arg, int fps, int simulate_infinite_loop);
void emscripten_cancel_main_loop(void);
#endif
