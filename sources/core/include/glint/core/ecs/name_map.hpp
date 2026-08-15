#pragma once

#include <cassert>
#include <optional>
#include <string>
#include <unordered_map>

#include <entt/fwd.hpp>

namespace glint::core {

class NameMap {
  public:
    NameMap() = default;

    auto set(std::string name, entt::entity e) -> void;
    [[nodiscard]]
    auto get(const std::string& name) const -> entt::entity;
    [[nodiscard]]
    auto get_name(entt::entity e) const noexcept -> std::optional<std::string>;
    auto rename(entt::entity e, std::string new_name) -> void;
    [[nodiscard]]
    auto exists(const std::string& name) const noexcept -> bool;
    auto remove(const std::string& name) -> void;

  private:
    std::unordered_map<std::string, entt::entity> m_name_map {};
    std::unordered_map<entt::entity, std::string> m_entity_map {};
};

class NameComp {
  public:
    static auto on_construct(entt::registry& reg, const entt::entity e) -> void;
    static auto on_update(entt::registry& reg, const entt::entity e) -> void;
    static auto on_destroy(entt::registry& reg, const entt::entity e) -> void;

    static auto register_lua(entt::registry& reg) -> void;

    NameComp(std::string name) noexcept;
    [[nodiscard]]
    auto name() const noexcept -> const std::string&;

  private:
    std::string m_name;
};

auto init_name_map(entt::registry& reg) -> void;

} // namespace glint::core
