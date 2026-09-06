#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <variant>

#include <entt/entity/registry.hpp>

namespace glint {

using ModuleValue = std::variant<std::monostate, std::string, int64_t, double>;

using ModuleConfig = std::unordered_map<std::string, ModuleValue>;

using ModuleHook = std::function<void(entt::registry& reg)>;

struct ModuleHooks {
    std::function<void(entt::registry&, const ModuleConfig&)> import;
    ModuleHook load;
    ModuleHook unload;
    ModuleHook update;
    ModuleHook render;
};

struct Module {
    std::string name = "";
    ModuleConfig config = {};
    ModuleHooks hooks = {};

    auto import(entt::registry& registry) const -> void;
};

struct ModuleContext {
    std::unordered_map<std::string, Module> modules {};
};

inline auto Module::import(entt::registry& registry) const -> void {
    auto& ctx = registry.ctx().emplace<ModuleContext>();
    if (ctx.modules.contains(name)) return;
    ctx.modules.emplace(name, *this);

    if (hooks.import) {
        hooks.import(registry, config);
    }
}

} // namespace glint
