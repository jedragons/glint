#include <glint/glint.h>

#include <stdio.h>

static void glint__main_done(galloc_Allocator alloc, glint_Game* game, bool ok, void* user);

int main(int argc, char** argv) {
    galloc_Allocator alloc = galloc_malloc_allocator();
    glint_Config config = glint_main(argc, argv);
    if (config.headless) {
        fprintf(stderr, "glint: the default entry point requires a graphical game\n");
        return 1;
    }

    glint_Game* game = glint_new(alloc, &config);
    if (game == NULL) {
        return 1;
    }

    return glint_run_with_completion(alloc, game, glint__main_done, NULL) ? 0 : 1;
}

static void glint__main_done(galloc_Allocator alloc, glint_Game* game, bool ok, void* user) {
    (void)user;
    if (!ok) {
        fprintf(stderr, "glint: %s\n", glint_error(game));
    }

    glint_destroy(alloc, game);
}
