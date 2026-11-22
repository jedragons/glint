#include <glint/json/json.h>
#include <glint/modules/dim2/atlas.h>

#include <stdint.h>
#include <string.h>

static char* gdim2_atlas__atlas_copy_name(galloc_Allocator alloc, const char* name, size_t length);
static bool gdim2_atlas__atlas_frame_number(const gjson_Value* object, const char* key, float* out);
static bool gdim2_atlas__atlas_read_frames(galloc_Allocator alloc, gdim2_atlas_Atlas* atlas, const gjson_Value* object);
static bool gdim2_atlas__atlas_read_animations(
    galloc_Allocator alloc,
    gdim2_atlas_Atlas* atlas,
    const gjson_Value* object
);

void gdim2_atlas_destroy(galloc_Allocator alloc, gdim2_atlas_Atlas* atlas) {
    if (atlas == NULL) {
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
    *atlas = (gdim2_atlas_Atlas) {0};
}

const gdim2_atlas_Frame* gdim2_atlas_frame(const gdim2_atlas_Atlas* atlas, const char* name) {
    if (atlas == NULL || name == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < atlas->frame_count; i++) {
        if (atlas->frames[i].name != NULL && strcmp(atlas->frames[i].name, name) == 0) {
            return &atlas->frames[i];
        }
    }

    return NULL;
}

const gdim2_atlas_Animation* gdim2_atlas_animation(const gdim2_atlas_Atlas* atlas, const char* name) {
    if (atlas == NULL || name == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < atlas->animation_count; i++) {
        if (atlas->animations[i].name != NULL && strcmp(atlas->animations[i].name, name) == 0) {
            return &atlas->animations[i];
        }
    }

    return NULL;
}

bool gdim2_atlas_parse(galloc_Allocator alloc, const char* json, size_t length, gdim2_atlas_Atlas* out) {
    if (out == NULL) {
        return false;
    }

    *out = (gdim2_atlas_Atlas) {0};
    if (json == NULL) {
        return false;
    }

    gjson_Value* root = gjson_parse(alloc, json, length, NULL);
    if (root == NULL) {
        return false;
    }

    bool success = false;
    if (root->type != GJSON_OBJECT) {
        goto cleanup;
    }

    if (!gdim2_atlas__atlas_read_frames(alloc, out, gjson_Value_get(root, "frames"))) {
        goto cleanup;
    }

    if (!gdim2_atlas__atlas_read_animations(alloc, out, gjson_Value_get(root, "animations"))) {
        goto cleanup;
    }

    success = true;

cleanup:
    gjson_Value_free(alloc, root);
    if (!success) {
        gdim2_atlas_destroy(alloc, out);
    }

    return success;
}

gdim2_atlas_AnimationPlayer gdim2_atlas_AnimationPlayer_new(
    const gdim2_atlas_Atlas* atlas,
    const gdim2_atlas_Animation* animation
) {
    return (gdim2_atlas_AnimationPlayer) {
        .atlas = atlas,
        .animation = animation,
    };
}

const gdim2_atlas_Frame* gdim2_atlas_AnimationPlayer_frame(const gdim2_atlas_AnimationPlayer* player) {
    bool player_ready = player != NULL && player->atlas != NULL && player->animation != NULL;
    if (!player_ready) {
        return NULL;
    }

    bool frame_in_bounds = player->current_frame < player->animation->count;
    if (!frame_in_bounds) {
        return NULL;
    }

    size_t index = player->animation->frames[player->current_frame];
    if (index >= player->atlas->frame_count) {
        return NULL;
    }

    return &player->atlas->frames[index];
}

void gdim2_atlas_AnimationPlayer_update(gdim2_atlas_AnimationPlayer* player, float dt) {
    const gdim2_atlas_Frame* frame = gdim2_atlas_AnimationPlayer_frame(player);
    if (frame == NULL) {
        return;
    }

    player->time += dt;
    if (player->time > frame->duration) {
        player->time = 0;
        player->current_frame = (player->current_frame + 1) % player->animation->count;
    }
}

static char* gdim2_atlas__atlas_copy_name(galloc_Allocator alloc, const char* name, size_t length) {
    if (name == NULL || length == SIZE_MAX || memchr(name, '\0', length) != NULL) {
        return NULL;
    }

    char* copy = galloc_alloc(alloc, length + 1);
    if (copy != NULL) {
        memcpy(copy, name, length);
        copy[length] = '\0';
    }

    return copy;
}

static bool gdim2_atlas__atlas_frame_number(const gjson_Value* object, const char* key, float* out) {
    const gjson_Value* value = gjson_Value_get(object, key);
    if (value == NULL) {
        *out = 0;
        return true;
    }

    if (value->type != GJSON_NUMBER) {
        return false;
    }

    double number = value->as.number;
    bool in_range = number >= 0 && number <= UINT32_MAX;
    if (!in_range) {
        return false;
    }

    bool is_integer = (double)(uint32_t)number == number;
    if (!is_integer) {
        return false;
    }
    *out = (float)value->as.number;
    return true;
}

static bool gdim2_atlas__atlas_read_frames(
    galloc_Allocator alloc,
    gdim2_atlas_Atlas* atlas,
    const gjson_Value* object
) {
    if (object == NULL) {
        return true;
    }

    if (object->type != GJSON_OBJECT || object->as.children.length > SIZE_MAX / sizeof(*atlas->frames)) {
        return false;
    }

    atlas->frame_count = object->as.children.length;
    if (atlas->frame_count > 0) {
        atlas->frames = galloc_calloc(alloc, atlas->frame_count, sizeof(*atlas->frames));
        if (atlas->frames == NULL) {
            atlas->frame_count = 0;
            return false;
        }
    }

    const gjson_Value* item = object->as.children.first;
    for (size_t i = 0; i < atlas->frame_count; i++, item = item->next) {
        gdim2_atlas_Frame* frame = &atlas->frames[i];
        if (item->type != GJSON_OBJECT || gdim2_atlas_frame(atlas, item->key) != NULL) {
            return false;
        }

        frame->name = gdim2_atlas__atlas_copy_name(alloc, item->key, item->key_length);
        bool frame_valid = (frame->name != NULL) && gdim2_atlas__atlas_frame_number(item, "x", &frame->x)
            && gdim2_atlas__atlas_frame_number(item, "y", &frame->y)
            && gdim2_atlas__atlas_frame_number(item, "w", &frame->width)
            && gdim2_atlas__atlas_frame_number(item, "h", &frame->height)
            && gdim2_atlas__atlas_frame_number(item, "duration", &frame->duration);
        if (!frame_valid) {
            return false;
        }

        frame->duration *= 0.001f;
    }

    return true;
}

static bool gdim2_atlas__atlas_read_animations(
    galloc_Allocator alloc,
    gdim2_atlas_Atlas* atlas,
    const gjson_Value* object
) {
    if (object == NULL) {
        return true;
    }

    if (object->type != GJSON_OBJECT || object->as.children.length > SIZE_MAX / sizeof(*atlas->animations)) {
        return false;
    }

    atlas->animation_count = object->as.children.length;
    if (atlas->animation_count > 0) {
        atlas->animations = galloc_calloc(alloc, atlas->animation_count, sizeof(*atlas->animations));
        if (atlas->animations == NULL) {
            atlas->animation_count = 0;
            return false;
        }
    }

    const gjson_Value* item = object->as.children.first;
    for (size_t i = 0; i < atlas->animation_count; i++, item = item->next) {
        gdim2_atlas_Animation* animation = &atlas->animations[i];
        if (item->type != GJSON_ARRAY) {
            return false;
        }

        bool duplicate_name = gdim2_atlas_animation(atlas, item->key) != NULL;
        bool frames_fit = item->as.children.length <= SIZE_MAX / sizeof(*animation->frames);
        if (duplicate_name || !frames_fit) {
            return false;
        }

        animation->name = gdim2_atlas__atlas_copy_name(alloc, item->key, item->key_length);
        if (animation->name == NULL) {
            return false;
        }

        animation->count = item->as.children.length;
        if (animation->count > 0) {
            animation->frames = galloc_alloc(alloc, animation->count * sizeof(*animation->frames));
            if (animation->frames == NULL) {
                return false;
            }
        }

        const gjson_Value* ref = item->as.children.first;
        for (size_t j = 0; j < animation->count; j++, ref = ref->next) {
            if (ref->type != GJSON_STRING || memchr(ref->as.string.data, '\0', ref->as.string.length) != NULL) {
                return false;
            }

            const gdim2_atlas_Frame* frame = gdim2_atlas_frame(atlas, ref->as.string.data);
            if (frame == NULL) {
                return false;
            }

            animation->frames[j] = (size_t)(frame - atlas->frames);
        }
    }

    return true;
}
