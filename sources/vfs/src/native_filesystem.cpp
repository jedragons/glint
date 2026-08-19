#include <glint/vfs/native_filesystem.hpp>

#include <fstream>

namespace glint::vfs {

NativeFileSystem::NativeFileSystem(std::filesystem::path point, std::filesystem::path root) :
    m_point(std::move(point)),
    m_root(std::move(root)) {}

auto NativeFileSystem::list(const std::string& path) -> std::vector<std::string> {
    std::vector<std::string> paths;
    for (auto file : std::filesystem::directory_iterator {final_path(path)}) {
        auto path = (m_point / file.path().lexically_relative(m_root)).lexically_normal();
        paths.emplace_back(path.string());
    }

    return paths;
}

auto NativeFileSystem::exists(const std::string& path) -> bool {
    return std::filesystem::exists(final_path(path));
}

auto NativeFileSystem::read(const std::string& path) -> std::vector<char> {
    auto file = std::ifstream {final_path(path), std::ios::binary};
    return {std::istreambuf_iterator<char>(file), {}};
}

auto NativeFileSystem::read_str(const std::string& path) -> std::string {
    auto file = std::ifstream {final_path(path)};
    return {std::istreambuf_iterator<char>(file), {}};
}

auto NativeFileSystem::real_path(const std::string& path) -> std::string {
    return std::filesystem::canonical(final_path(path)).string();
}

auto NativeFileSystem::final_path(const std::string& path) const -> std::filesystem::path {
    auto p = std::filesystem::path {path};
    auto result = m_root / p.lexically_relative(m_point);
    return result;
}

} // namespace glint::vfs
