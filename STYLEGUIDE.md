# Glint style guide

This guide applies to code maintained here. Third-party code keeps its own style.
Follow these rules in new and edited code; older code may still need to catch up.

## S: Files and documentation

### S.1: Keep related headers and source files together

Place tests under `tests/`, following the engine's directory structure.

### S.2: Put the public implementation before private helpers

Order a `.c` file as follows:

```c
#include "./arena.h"

static void* garena__allocate(garena_Arena* arena, size_t size);

// Public implementations
// ...

static void* garena__allocate(garena_Arena* arena, size_t size) {
    // ...
}
```

Private types and other declarations go before the function implementations.
The example labels illustrate the order; they need not appear in source files.

### S.3: Make headers self-contained

Include what the header needs so it can be included on its own.

### S.4: Use `#pragma once` in headers

### S.5: Keep C++ linkage guards in public C headers

### S.6: Separate project and standard library includes with an empty line

### S.7: Document useful contracts in headers

Use `///` for behavior the declaration does not explain. Skip comments that only
repeat a name. Start with a short summary; use Rust-style `# Ownership`, `# Errors`
and `# Examples` sections when needed. Omit irrelevant sections and keep each one brief.

```c
/// Create an entity
///
/// # Errors
///
/// Returns an entity with ID zero on failure.
gecs_Entity gecs_spawn(gecs_World* world);
```

### S.8: Preserve useful comments

Explain why implementation steps are necessary. Update a comment when its code
changes or it is wrong; preserve it otherwise.

### S.9: Keep platform details behind the platform interface

Use `glint/config.h` for platform and compiler checks.

### S.10: Keep the engine's external dependencies limited to Raylib and Lua

### S.11: Use `//` for general comments

Use consecutive `//` lines for multiline comments. Reserve `/* ... */` for very
large comments of about 100 lines or more. Public API documentation uses `///`.

## N: Naming

### N.1: Use consistent names

| Code element                      | Convention                            | Example                                   |
| --------------------------------- | ------------------------------------- | ----------------------------------------- |
| Types                             | Prefix followed by `PascalCase`       | `gstr_Buffer`, `gecs_ID`                  |
| Public functions                  | Prefix followed by `snake_case`       | `garena_reset`                            |
| Type-specific functions           | Type name followed by `snake_case`    | `gstr_Buffer_clear`                       |
| Private symbols                   | Double underscore after the prefix    | `galloc__malloc_alloc`                    |
| Variables, parameters, and fields | `snake_case`                          | `frame_count`, `block_size`               |
| Macros and enum constants         | Prefix followed by `UPPER_SNAKE_CASE` | `GARENA_DEFAULT_BLOCK_SIZE`, `GJSON_NULL` |

### N.2: Give each implementation unit its own prefix

Keep prefixes distinct even when the files are combined in a unity build.
Header-only helpers also need a prefix, such as `gvec2_`.

### N.3: Mark private functions `static`

A private name still needs internal linkage.

## F: Formatting

### F.1: Follow `.clang-format`

Use four spaces, no tabs, attached braces, and a 120-column limit.

### F.2: Attach pointer stars to the type

Write `const char* str` and `void** ptr`.

### F.3: Separate functions with an empty line

Apply this to header declarations as well as implementations.

### F.4: Use empty lines between logical steps

Give validation, allocation, processing, and cleanup room to read.

### F.5: Keep short guard clauses readable

Expand longer branches into braced blocks.

## A: Types, APIs, and ownership

### A.1: Use `bool` for boolean values

Include `<stdbool.h>` rather than representing success or flags with integers.

### A.2: Use explicit integer types

Use fixed-width types from `<stdint.h>`.

### A.3: Use `size_t` for sizes, counts, and memory indices

### A.4: Match external API types at their boundaries

For example, store the result of `vsnprintf` in an `int`.

### A.5: Declare variables near their first use

### A.6: Prefer one declaration per statement

### A.7: Use `const` for read-only data

### A.8: Pass small value objects by value

This includes slices, allocators, and ECS entities. Use pointers for mutable
objects and opaque state.

### A.9: Use an entity when both a world and an ID are needed

`gecs_Entity` contains both. `gecs_ID` is the raw world-local identifier.

### A.10: Make zero initialization useful where practical

Avoid an init function whose only job is to zero a value.

### A.11: Use designated initializers when field names clarify the meaning

```c
gstr_Buffer text = {0};
gecs_Entity entity = {.world = world, .id = id};
```

### A.12: Make the allocation source explicit

Allocating APIs receive an allocator. An API that already receives a world obtains
its allocator from that world and does not need a separate allocator parameter.

### A.13: Use stored allocator state when the object owns it

An arena retains its backing allocator, so destroying it only requires the arena.

### A.14: Release memory with the allocator that allocated it

Keep the allocator and its state alive for as long as their allocations need it.

### A.15: Keep temporary allocations within their lifetime

World temporary allocations expire at frame end. Do not retain them in persistent
state.

## C: Expressions and error handling

### C.1: Compare pointers and numbers explicitly

Use `ptr != NULL`, `count > 0`, or `count == 0`. Use `value != 0` when negative
values also count as nonzero. Boolean values can be tested directly.

### C.2: Give complex conditions descriptive names

Keep simple comparisons inline. Extract conditions when a name helps explain what
the expression means.

```c
bool high_surrogate = cp >= 0xd800 && cp <= 0xdbff;
if (high_surrogate) {
    // Decode the second escape.
}
```

### C.3: Preserve the order of safety checks

Check pointers and bounds before accessing memory. Extracting conditions must not
introduce an earlier dereference or array access.

### C.4: Keep assignments out of conditions

Read or update the value in a separate statement, then test it.

### C.5: Use early returns to limit nesting

Handle invalid input or a completed case before the main work.

### C.6: Use assertions for preconditions and invariants

### C.7: Report runtime failures through return values or error outputs

Allocation failure and invalid input are runtime failures.

### C.8: Clean up partially constructed resources

Use a cleanup label when several failure paths release the same resources.

### C.9: Prefer explicit increments and decrements

Use `count += 1` and `count -= 1` instead of `++` and `--`. The exception is the
update expression of a `for` loop. Keep other updates in their own statements.

## V: Testing and performance

### V.1: Test observable behavior and failure handling

Cover meaningful edge cases. Give checks descriptions that explain a failure.

### V.2: Run checks appropriate to the change

Use sanitizers to check memory errors and undefined behavior.

### V.3: Measure before optimizing

Use benchmarks or profiling to identify work worth optimizing. Base performance
claims on measurements.

### V.4: Use sections for alternative outcomes that share a fixture

Use `GUNIT_SETUP` for the starting state and give each `GUNIT_SECTION` a name
that explains what changes or what to expect. For example, a loaded asset can
reload successfully or keep its previous contents after a failed reload. Each
section gets a fresh run of the setup. Keep a single sequence, such as checking
the order of destruction, in a standalone test when it reads clearly on its own.

Declare fixture variables borrowed by deferred cleanup directly in the
`GUNIT_TEST` body so they stay alive until cleanup runs. Register cleanup in
`GUNIT_SETUP` when the setup acquires the resource.
