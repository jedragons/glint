#pragma once

#include <glint/core/alloc.h>

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gdim2_atlas_Frame {
    char* name;
    float x;
    float y;
    float width;
    float height;
    float duration; // seconds
} gdim2_atlas_Frame;

typedef struct gdim2_atlas_Animation {
    char* name;
    size_t* frames; // indices into gdim2_atlas_Atlas.frames
    size_t count;
} gdim2_atlas_Animation;

typedef struct gdim2_atlas_Atlas {
    gdim2_atlas_Frame* frames;
    size_t frame_count;
    gdim2_atlas_Animation* animations;
    size_t animation_count;
} gdim2_atlas_Atlas;

typedef struct gdim2_atlas_AnimationPlayer {
    const gdim2_atlas_Atlas* atlas;
    const gdim2_atlas_Animation* animation;
    size_t current_frame;
    float time;
} gdim2_atlas_AnimationPlayer;

/// Parse an atlas into an empty output.
///
/// # Ownership
///
/// Free the result with `gdim2_atlas_destroy` using the same allocator.
///
/// # Errors
///
/// Returns false and zeroes the output on failure.
bool gdim2_atlas_parse(galloc_Allocator alloc, const char* json, size_t length, gdim2_atlas_Atlas* out);

void gdim2_atlas_destroy(galloc_Allocator alloc, gdim2_atlas_Atlas* atlas);

const gdim2_atlas_Frame* gdim2_atlas_frame(const gdim2_atlas_Atlas* atlas, const char* name);

const gdim2_atlas_Animation* gdim2_atlas_animation(const gdim2_atlas_Atlas* atlas, const char* name);

/// The player borrows its atlas; the atlas must outlive it.
gdim2_atlas_AnimationPlayer gdim2_atlas_AnimationPlayer_new(
    const gdim2_atlas_Atlas* atlas,
    const gdim2_atlas_Animation* animation
);

const gdim2_atlas_Frame* gdim2_atlas_AnimationPlayer_frame(const gdim2_atlas_AnimationPlayer* player);

void gdim2_atlas_AnimationPlayer_update(gdim2_atlas_AnimationPlayer* player, float dt);

#ifdef __cplusplus
}
#endif
