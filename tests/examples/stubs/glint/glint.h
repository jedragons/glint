#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void* data;
} galloc_Allocator;
typedef struct glint_Game glint_Game;
typedef void (*glint_RunEndFn)(galloc_Allocator, glint_Game*, bool, void*);

typedef struct {
    const char* asset_root;
    const char* initial_script;
    int width;
    int height;
    const char* title;
    bool headless;
} glint_Config;

glint_Config glint_main(int argc, char** argv);

galloc_Allocator galloc_malloc_allocator(void);
glint_Game* glint_new(galloc_Allocator, const glint_Config*);
bool glint_destroy(galloc_Allocator, glint_Game*);
bool glint_run(galloc_Allocator, glint_Game*);
bool glint_run_with_completion(galloc_Allocator, glint_Game*, glint_RunEndFn, void*);
bool glint_frame(galloc_Allocator, glint_Game*, float);
const char* glint_error(const glint_Game*);

#ifdef __cplusplus
}
#endif
