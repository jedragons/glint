// Keep the watcher first: its POSIX feature macro must precede system headers.
// clang-format off
#include "./watcher.c"
#include "./filesystem.c"
// clang-format on
