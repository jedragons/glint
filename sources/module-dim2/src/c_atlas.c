#include <glint/dim2/atlas.h>
#include <glint/json.h>

#include <stdint.h>
#include <string.h>

static char *gatlas__atlas_copy_name(galloc_Allocator alloc, const char *name, size_t length) {
    if (!name || length == SIZE_MAX || memchr(name, '\0', length)) {
        return NULL;
    }
    char *copy = galloc_alloc(alloc, length + 1);
    if (copy) {
        memcpy(copy, name, length);
        copy[length] = '\0';
    }
    return copy;
}

void gatlas_destroy(galloc_Allocator alloc, gatlas_Atlas *atlas) {
    if (!atlas) {
        return;
    }
    for (size_t i = 0; i < atlas->frame_count; i++) {
        galloc_free(alloc, atlas->frames[i].name);
    }
    for (size_t i = 0; i < atlas->animation_count; i++) {
        galloc_free(alloc, atlas->animations[i].name);
        galloc_free(alloc, atlas->animations[i].frames);
    }
    galloc_free(alloc, atlas->frames);
    galloc_free(alloc, atlas->animations);
    *atlas = (gatlas_Atlas) {0};
}

const gatlas_Frame *gatlas_frame(const gatlas_Atlas *atlas, const char *name) {
    if (!atlas || !name) {
        return NULL;
    }
    for (size_t i = 0; i < atlas->frame_count; i++) {
        if (atlas->frames[i].name && strcmp(atlas->frames[i].name, name) == 0) {
            return &atlas->frames[i];
        }
    }
    return NULL;
}

const gatlas_Animation *gatlas_animation(const gatlas_Atlas *atlas, const char *name) {
    if (!atlas || !name) {
        return NULL;
    }
    for (size_t i = 0; i < atlas->animation_count; i++) {
        if (atlas->animations[i].name && strcmp(atlas->animations[i].name, name) == 0) {
            return &atlas->animations[i];
        }
    }
    return NULL;
}

static bool gatlas__atlas_frame_number(const gjson_Value *object, const char *key, float *out) {
    const gjson_Value *value = gjson_Value_get(object, key);
    if (!value) {
        *out = 0;
        return true;
    }
    if (value->type != GJSON_NUMBER || !(value->as.number >= 0 && value->as.number <= UINT32_MAX) ||
        (double)(uint32_t)value->as.number != value->as.number) {
        return false;
    }
    *out = (float)value->as.number;
    return true;
}

static bool gatlas__atlas_read_frames(galloc_Allocator alloc, gatlas_Atlas *atlas, const gjson_Value *object) {
    if (!object) {
        return true;
    }
    if (object->type != GJSON_OBJECT || object->as.children.length > SIZE_MAX / sizeof(*atlas->frames)) {
        return false;
    }
    atlas->frame_count = object->as.children.length;
    if (atlas->frame_count) {
        atlas->frames = galloc_calloc(alloc, atlas->frame_count, sizeof(*atlas->frames));
        if (!atlas->frames) {
            atlas->frame_count = 0;
            return false;
        }
    }
    const gjson_Value *item = object->as.children.first;
    for (size_t i = 0; i < atlas->frame_count; i++, item = item->next) {
        gatlas_Frame *frame = &atlas->frames[i];
        if (item->type != GJSON_OBJECT || gatlas_frame(atlas, item->key)) {
            return false;
        }
        frame->name = gatlas__atlas_copy_name(alloc, item->key, item->key_length);
        if (!frame->name || !gatlas__atlas_frame_number(item, "x", &frame->x) || !gatlas__atlas_frame_number(item, "y", &frame->y) ||
            !gatlas__atlas_frame_number(item, "w", &frame->width) || !gatlas__atlas_frame_number(item, "h", &frame->height) ||
            !gatlas__atlas_frame_number(item, "duration", &frame->duration)) {
            return false;
        }
        frame->duration *= 0.001f;
    }
    return true;
}

static bool gatlas__atlas_read_animations(galloc_Allocator alloc, gatlas_Atlas *atlas, const gjson_Value *object) {
    if (!object) {
        return true;
    }
    if (object->type != GJSON_OBJECT || object->as.children.length > SIZE_MAX / sizeof(*atlas->animations)) {
        return false;
    }
    atlas->animation_count = object->as.children.length;
    if (atlas->animation_count) {
        atlas->animations = galloc_calloc(alloc, atlas->animation_count, sizeof(*atlas->animations));
        if (!atlas->animations) {
            atlas->animation_count = 0;
            return false;
        }
    }
    const gjson_Value *item = object->as.children.first;
    for (size_t i = 0; i < atlas->animation_count; i++, item = item->next) {
        gatlas_Animation *animation = &atlas->animations[i];
        if (item->type != GJSON_ARRAY || gatlas_animation(atlas, item->key) ||
            item->as.children.length > SIZE_MAX / sizeof(*animation->frames)) {
            return false;
        }
        animation->name = gatlas__atlas_copy_name(alloc, item->key, item->key_length);
        if (!animation->name) {
            return false;
        }
        animation->count = item->as.children.length;
        if (animation->count) {
            animation->frames = galloc_alloc(alloc, animation->count * sizeof(*animation->frames));
            if (!animation->frames) {
                return false;
            }
        }
        const gjson_Value *ref = item->as.children.first;
        for (size_t j = 0; j < animation->count; j++, ref = ref->next) {
            if (ref->type != GJSON_STRING || memchr(ref->as.string.data, '\0', ref->as.string.length)) {
                return false;
            }
            const gatlas_Frame *frame = gatlas_frame(atlas, ref->as.string.data);
            if (!frame) {
                return false;
            }
            animation->frames[j] = (size_t)(frame - atlas->frames);
        }
    }
    return true;
}

bool gatlas_parse(galloc_Allocator alloc, const char *json, size_t length, gatlas_Atlas *out) {
    if (!out) {
        return false;
    }
    *out = (gatlas_Atlas) {0};
    if (!json) {
        return false;
    }
    gjson_Value *root = gjson_parse(alloc, json, length, NULL);
    if (!root) {
        return false;
    }
    bool success = root->type == GJSON_OBJECT &&
                   gatlas__atlas_read_frames(alloc, out, gjson_Value_get(root, "frames")) &&
                   gatlas__atlas_read_animations(alloc, out, gjson_Value_get(root, "animations"));
    gjson_Value_free(alloc, root);
    if (!success) {
        gatlas_destroy(alloc, out);
    }
    return success;
}

gatlas_AnimationPlayer gatlas_AnimationPlayer_new(const gatlas_Atlas *atlas, const gatlas_Animation *animation) {
    return (gatlas_AnimationPlayer) {.atlas = atlas, .animation = animation};
}

const gatlas_Frame *gatlas_AnimationPlayer_frame(const gatlas_AnimationPlayer *player) {
    if (!player || !player->atlas || !player->animation || player->current_frame >= player->animation->count) {
        return NULL;
    }
    size_t index = player->animation->frames[player->current_frame];
    if (index >= player->atlas->frame_count) {
        return NULL;
    }
    return &player->atlas->frames[index];
}

void gatlas_AnimationPlayer_update(gatlas_AnimationPlayer *player, float dt) {
    const gatlas_Frame *frame = gatlas_AnimationPlayer_frame(player);
    if (!frame) {
        return;
    }
    player->time += dt;
    if (player->time > frame->duration) {
        player->time = 0;
        player->current_frame = (player->current_frame + 1) % player->animation->count;
    }
}
