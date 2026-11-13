#ifndef GATLAS_DIM2_ATLAS_H_
#define GATLAS_DIM2_ATLAS_H_

#include <glint/alloc.h>

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gatlas_Frame {
    char *name;
    float x;
    float y;
    float width;
    float height;
    float duration; /* seconds */
} gatlas_Frame;

typedef struct gatlas_Animation {
    char *name;
    size_t *frames; /* indices into gatlas_Atlas.frames */
    size_t count;
} gatlas_Animation;

typedef struct gatlas_Atlas {
    gatlas_Frame *frames;
    size_t frame_count;
    gatlas_Animation *animations;
    size_t animation_count;
} gatlas_Atlas;

typedef struct gatlas_AnimationPlayer {
    const gatlas_Atlas *atlas;
    const gatlas_Animation *animation;
    size_t current_frame;
    float time;
} gatlas_AnimationPlayer;

/* Parses a JSON atlas from memory. The result is owned by the caller and must
 * be freed with the same allocator. On failure the result is zeroed. The
 * output must not already own an atlas; destroy it before parsing again. */
bool gatlas_parse(galloc_Allocator alloc, const char *json, size_t length, gatlas_Atlas *out);
void gatlas_destroy(galloc_Allocator alloc, gatlas_Atlas *atlas);
const gatlas_Frame *gatlas_frame(const gatlas_Atlas *atlas, const char *name);
const gatlas_Animation *gatlas_animation(const gatlas_Atlas *atlas, const char *name);
/* Players borrow the atlas, which must outlive them. */
gatlas_AnimationPlayer gatlas_AnimationPlayer_new(const gatlas_Atlas *atlas, const gatlas_Animation *animation);
const gatlas_Frame *gatlas_AnimationPlayer_frame(const gatlas_AnimationPlayer *player);
void gatlas_AnimationPlayer_update(gatlas_AnimationPlayer *player, float dt);

#ifdef __cplusplus
}
#endif
#endif // GATLAS_DIM2_ATLAS_H_
