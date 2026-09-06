#pragma once

#include <memory>
#include <string>

#include <entt/fwd.hpp>

#include <glint/core/assets/asset.hpp>

namespace glint::core {

template<is_asset Asset>
class Resource {
  public:
    Resource() = default;

    explicit Resource(std::shared_ptr<Asset> handle) : m_handle(handle) {}

    static auto load(const std::string& path, entt::registry& registry) {
        return Resource(std::make_shared<Asset>(Asset::load(path, registry)));
    }

    [[nodiscard]]
    auto handle() noexcept -> std::shared_ptr<Asset> {
        return m_handle;
    }

    auto swap(Resource& other) noexcept -> void {
        using std::swap;
        swap(m_handle, other.m_handle);
    }

    [[nodiscard]]
    auto operator*() const noexcept -> Asset& {
        return *m_handle;
    }

    [[nodiscard]]
    operator Asset&() const noexcept {
        return *m_handle;
    }

    [[nodiscard]]
    auto operator->() const noexcept -> Asset* {
        return m_handle.get();
    }

    [[nodiscard]]
    explicit operator bool() const noexcept {
        return static_cast<bool>(m_handle);
    }

    template<typename Other>
    [[nodiscard]]
    auto operator==(const Resource<Other>& other) const noexcept -> bool {
        return (m_handle == other.m_handle);
    }

    template<typename Other>
    [[nodiscard]]
    auto operator<=>(const Resource<Other>& other) const noexcept {
        return (m_handle <=> other.m_handle);
    }

    void reset() {
        m_handle.reset();
    }

    void reset(std::shared_ptr<Asset> other) {
        m_handle = std::move(other);
    }

  private:
    std::shared_ptr<Asset> m_handle {};
};

} // namespace glint::core
