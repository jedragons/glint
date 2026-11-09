#include <glint/platform/filesystem.hpp>

#include <raylib.h>

namespace glint::platform {

auto get_working_directory() -> std::string {
    return GetWorkingDirectory();
}

} // namespace glint::platform
