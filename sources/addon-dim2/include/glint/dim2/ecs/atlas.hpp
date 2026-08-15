#pragma once

#include <entt/resource/resource.hpp>

#include <glint/dim2/asset/atlas.hpp>

namespace glint::dim2 {

struct AtlasComp {
    entt::resource<AtlasAsset> atlas;
};

} // namespace glint::dim2
