#include <glint/core/ecs/name_map.hpp>

#include <entt/entity/registry.hpp>
#include <fmt/format.h>

namespace glint::core {

auto NameMap::set(std::string name, entt::entity e) -> void {
    if (m_name_map.contains(name)) {
        throw std::runtime_error(fmt::format("Entity with name `{}` already exists", name));
    }

    m_name_map.insert_or_assign(name, e);
    m_entity_map.insert_or_assign(e, name);
}

auto NameMap::get(const std::string& name) const -> entt::entity {
    auto e_it = m_name_map.find(name);
    if (e_it == m_name_map.end()) return entt::null;
    return e_it->second;
}

auto NameMap::get_name(entt::entity e) const noexcept -> std::optional<std::string> {
    auto name_it = m_entity_map.find(e);
    if (name_it == m_entity_map.end()) return std::nullopt;
    return name_it->second;
}

auto NameMap::rename(entt::entity e, std::string new_name) -> void {
    if (exists(new_name)) {
        throw std::runtime_error(fmt::format("Entity with name `{}` already exists", new_name));
    }

    auto old_name = get_name(e);
    if (!old_name) {
        throw std::runtime_error(fmt::format("Entity `{}` was not named", entt::to_integral(e)));
    }

    m_name_map.erase(*old_name);
    m_name_map.insert_or_assign(new_name, e);
    m_entity_map.insert_or_assign(e, new_name);
}

auto NameMap::exists(const std::string& name) const noexcept -> bool {
    return m_name_map.contains(name);
}

auto NameMap::remove(const std::string& name) -> void {
    if (!m_name_map.contains(name)) {
        throw std::runtime_error(fmt::format("Entity with name `{}` does not exist", name));
    }

    auto e = get(name);
    m_name_map.erase(name);
    m_entity_map.erase(e);
}

auto NameComp::on_construct(entt::registry& reg, const entt::entity e) -> void {
    assert(reg.all_of<NameComp>(e));
    const auto& comp = reg.get<NameComp>(e);
    reg.ctx().get<NameMap>().set(comp.m_name, e);
}

auto NameComp::on_update(entt::registry& reg, const entt::entity e) -> void {
    assert(reg.all_of<NameComp>(e));
    const auto& comp = reg.get<NameComp>(e);
    reg.ctx().get<NameMap>().rename(e, comp.m_name);
}

auto NameComp::on_destroy(entt::registry& reg, const entt::entity e) -> void {
    assert(reg.all_of<NameComp>(e));
    const auto& comp = reg.get<NameComp>(e);
    reg.ctx().get<NameMap>().remove(comp.m_name);
}

NameComp::NameComp(std::string name) noexcept : m_name(std::move(name)) {}

auto NameComp::name() const noexcept -> const std::string& {
    return m_name;
}

auto init_name_map(entt::registry& reg) -> void {
    reg.ctx().emplace<NameMap>();
}

} // namespace glint::core
