#pragma once

#include <glint/glint.h>

typedef struct gtest_ExampleGame {
    bool alive;
} gtest_ExampleGame;

typedef void (*gtest_ExampleRunEndFn)(galloc_Allocator alloc, gtest_ExampleGame* game, bool ok, void* user);
