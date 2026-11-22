// Native feature macros precede system headers. This TU never includes raylib.h.
// clang-format off
#include "../sources/glint/platform/native.c"
#include "../sources/glint/lib.c"
#include "../sources/glint/platform/logging.c"
#include "./platform/mock.c"
// clang-format on
