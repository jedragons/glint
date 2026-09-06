#pragma once

#include <string>
#include <unordered_map>

#include <sol/forward.hpp>

#include <glint/core/lua/asset.hpp>

namespace glint::core {

struct Behavior {
    Resource<LuaAsset> module;
    std::unordered_map<std::string, sol::object> data {};
};

} // namespace glint::core
