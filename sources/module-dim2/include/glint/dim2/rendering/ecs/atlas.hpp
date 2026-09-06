#pragma once

#include <glint/core/assets.hpp>
#include <glint/dim2/rendering/asset/atlas.hpp>

namespace glint::dim2 {

struct AtlasComp {
    core::Resource<AtlasAsset> atlas;
};

} // namespace glint::dim2
