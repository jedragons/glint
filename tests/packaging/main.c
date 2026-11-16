#include <glint/glint.h>

int main(int argc, char** argv) {
    (void)argv;
    grender_Texture texture = {0};
    gjson_Value value = {0};
    (void)texture;
    (void)value;
    // Exercise the full static link without opening a window when run normally.
    if (argc > 1) {
        galloc_Allocator alloc = galloc_malloc_allocator();
        glint_Config config = {0};
        glint_Game* game = glint_new(alloc, &config);
        glint_destroy(alloc, game);
    }

    return 0;
}
