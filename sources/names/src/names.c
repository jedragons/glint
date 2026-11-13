#include <glint/names.h>

#include <string.h>
#include <stdint.h>

typedef struct gname__Name {
    galloc_Allocator alloc;
    char *text;
} gname__Name;

static void gname__names_name_destroy(void *data) {
    gname__Name *name = data;
    galloc_free(name->alloc, name->text);
}

static gecs_Component gname__names_name_type(const gecs_World *world) {
    return gecs_Component_find(world, "GlintName");
}

gecs_Entity gname_find(const gecs_World *world, const char *name) {
    if (!world || !name) {
        return 0;
    }
    gecs_Component type = gname__names_name_type(world);
    size_t count = gecs_Component_count(world, type);
    for (size_t i = 0; i < count; i++) {
        gecs_Entity entity = gecs_Component_entity(world, type, i);
        const gname__Name *value = gecs_Component_get(world, entity, type);
        if (value && !strcmp(value->text, name)) {
            return entity;
        }
    }
    return 0;
}

const char *gname_get(const gecs_World *world, gecs_Entity entity) {
    const gname__Name *value = gecs_Component_get(world, entity, gname__names_name_type(world));
    return value ? value->text : NULL;
}

bool gname_set(galloc_Allocator alloc, gecs_World *world, gecs_Entity entity, const char *name) {
    if (!gecs_Entity_alive(world, entity)) {
        return false;
    }
    gecs_Component type = gname__names_name_type(world);
    if (!name) {
        if (type) {
            gecs_Component_remove(world, entity, type);
        }
        return true;
    }
    gecs_Entity existing = gname_find(world, name);
    if (existing && existing != entity) {
        return false;
    }
    if (!type) {
        type = gecs_Component_register(alloc, world, "GlintName", sizeof(gname__Name), gname__names_name_destroy);
        if (!type) {
            return false;
        }
    }
    size_t length = strlen(name);
    if (length == SIZE_MAX) {
        return false;
    }
    char *copy = galloc_alloc(alloc, length + 1);
    if (!copy) {
        return false;
    }
    memcpy(copy, name, length + 1);
    gname__Name value = {.alloc = alloc, .text = copy};
    if (!gecs_Component_set(alloc, world, entity, type, &value)) {
        galloc_free(alloc, copy);
        return false;
    }
    return true;
}

bool gname_exists(const gecs_World *world, const char *name) {
    return gname_find(world, name) != 0;
}

bool gname_rename(galloc_Allocator alloc, gecs_World *world, gecs_Entity entity, const char *new_name) {
    if (!new_name || !gname_get(world, entity)) {
        return false;
    }
    return gname_set(alloc, world, entity, new_name);
}

bool gname_remove(gecs_World *world, gecs_Entity entity) {
    if (!gecs_Entity_alive(world, entity) || !gname_get(world, entity)) {
        return false;
    }
    gecs_Component type = gname__names_name_type(world);
    return type && gecs_Component_remove(world, entity, type);
}
