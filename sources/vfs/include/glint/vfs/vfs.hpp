#pragma once

#include <algorithm>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <glint/vfs/filesystem.hpp>
#include <glint/vfs/fwd.hpp>
#include <glint/vfs/native_filesystem.hpp>

namespace glint::vfs {

class Vfs final {
  public:
    Vfs() = default;

    template<typename FS, typename... Args>
    auto mount(std::string point, Args... args) -> void;

    auto mount(std::string point, std::string path) -> void;

    auto list(const std::string& path) -> std::vector<std::string>;
    auto read(const std::string& path) -> std::vector<char>;
    auto read_str(const std::string& path) -> std::string;
    auto real_path(const std::string& path) -> std::string;

    [[nodiscard]]
    auto get_point_list() const -> const std::vector<std::string>& {
        return m_points;
    }

  private:
    std::unordered_map<std::string, std::vector<std::unique_ptr<IFileSystem>>> m_filesystems {};
    std::vector<std::string> m_points {};
    std::mutex m_mutex {};

    auto get_fs_list(const std::string& point) -> std::vector<std::unique_ptr<IFileSystem>>*;
};

template<typename FS, typename... Args>
inline auto Vfs::mount(std::string point, Args... args) -> void {
    [[maybe_unused]]
    auto lock = std::scoped_lock {m_mutex};

    m_filesystems[point].emplace_back(std::make_unique<FS>(args...));

    if (!std::ranges::contains(m_points, point)) {
        m_points.emplace_back(point);
        std::ranges::stable_sort(m_points, [](const auto& a, const auto& b) -> bool { return a > b; });
    }
}

} // namespace glint::vfs
