#include "./names_internal.h"

#include <stdint.h>
#include <string.h>

typedef struct gname__Name {
    galloc_Allocator alloc;
    char* text;
} gname__Name;

static void gname__destroy(void* data);
static gecs_Component gname__type(const gecs_World* world);

gecs_Component gname__register(gecs_World* world) {
    return gecs_Component_register(world, "GlintName", sizeof(gname__Name), gname__destroy);
}

gecs_Entity gname_find(gecs_World* world, const char* name) {
    if (world == NULL || name == NULL) {
        return (gecs_Entity) {0};
    }

    gecs_Component type = gname__type(world);
    size_t count = gecs_Component_count(world, type);
    for (size_t i = 0; i < count; i++) {
        gecs_Entity entity = gecs_Component_entity(world, type, i);
        const gname__Name* value = gecs_Component_get(entity, type);
        if (value != NULL && strcmp(value->text, name) == 0) {
            return entity;
        }
    }

    return (gecs_Entity) {0};
}

const char* gname_get(gecs_Entity handle) {
    gecs_World* world = handle.world;
    const gname__Name* value = gecs_Component_get(handle, gname__type(world));
    return value != NULL ? value->text : NULL;
}

bool gname_set(gecs_Entity handle, const char* name) {
    gecs_World* world = handle.world;
    gecs_ID entity = handle.id;
    galloc_Allocator alloc = glint_allocator(world);
    if (!gecs_Entity_alive(handle)) {
        return false;
    }

    gecs_Component type = gname__type(world);
    if (name == NULL) {
        if (type > 0) {
            gecs_Component_remove(handle, type);
        }

        return true;
    }

    gecs_ID existing = gname_find(world, name).id;
    if (existing > 0 && existing != entity) {
        return false;
    }

    if (type == 0) {
        type = gname__register(world);
        if (type == 0) {
            return false;
        }
    }

    size_t length = strlen(name);
    if (length == SIZE_MAX) {
        return false;
    }

    char* copy = galloc_alloc(alloc, length + 1);
    if (copy == NULL) {
        return false;
    }

    memcpy(copy, name, length + 1);
    gname__Name value = {.alloc = alloc, .text = copy};
    if (gecs_Component_set(handle, type, &value) == NULL) {
        galloc_free(alloc, copy);
        return false;
    }

    return true;
}

bool gname_exists(gecs_World* world, const char* name) {
    return gname_find(world, name).id != 0;
}

bool gname_rename(gecs_Entity handle, const char* new_name) {
    if (new_name == NULL || gname_get(handle) == NULL) {
        return false;
    }

    return gname_set(handle, new_name);
}

bool gname_remove(gecs_Entity handle) {
    gecs_World* world = handle.world;
    if (!gecs_Entity_alive(handle) || gname_get(handle) == NULL) {
        return false;
    }

    gecs_Component type = gname__type(world);
    return type != 0 && gecs_Component_remove(handle, type);
}

static void gname__destroy(void* data) {
    gname__Name* name = data;
    galloc_free(name->alloc, name->text);
}

static gecs_Component gname__type(const gecs_World* world) {
    return gecs_Component_find(world, "GlintName");
}
