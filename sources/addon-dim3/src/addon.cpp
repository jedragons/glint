#include <glint/dim3/addon.hpp>

namespace glint {

auto create_dim3_addon() -> NativeAddon {
    return {.name = "glint-dim3"};
}

} // namespace glint
