#include <glint/vfs/vfs.hpp>

namespace glint::vfs {

auto Vfs::mount(std::string point, std::string path) -> void {
    mount<NativeFileSystem>(point, point, path);
}

auto Vfs::list(const std::string& path) -> std::vector<std::string> {
    auto files = std::vector<std::string> {};
    for (const auto& point : m_points) {
        if (!path.starts_with(point)) continue;

        auto fs_list = get_fs_list(point);
        if (fs_list == nullptr) continue;

        for (auto& fs : *fs_list) {
            for (auto entry : fs->list(path)) {
                files.emplace_back(std::move(entry));
            }
        }
    }

    return files;
}

auto Vfs::read(const std::string& path) -> std::vector<char> {
    for (const auto& point : m_points) {
        if (!path.starts_with(point)) continue;

        auto fs_list = get_fs_list(point);
        if (fs_list == nullptr) continue;

        for (auto& fs : *fs_list) {
            if (!fs->exists(path)) continue;
            return fs->read(path);
        }
    }
    return {};
}

auto Vfs::read_str(const std::string& path) -> std::string {
    for (const auto& point : m_points) {
        if (!path.starts_with(point)) continue;

        auto fs_list = get_fs_list(point);
        if (fs_list == nullptr) continue;

        for (auto& fs : *fs_list) {
            if (!fs->exists(path)) continue;
            return fs->read_str(path);
        }
    }
    return {};
}

auto Vfs::real_path(const std::string& path) -> std::string {
    for (const auto& point : m_points) {
        if (!path.starts_with(point)) continue;

        auto fs_list = get_fs_list(point);
        if (fs_list == nullptr) continue;

        for (auto& fs : *fs_list) {
            if (!fs->exists(path)) continue;
            return fs->real_path(path);
        }
    }
    return {};
}

auto Vfs::get_fs_list(const std::string& point) -> std::vector<std::unique_ptr<IFileSystem>>* {
    if (auto it = m_filesystems.find(point); it != m_filesystems.end()) {
        return &it->second;
    } else {
        return nullptr;
    }
}

} // namespace glint::vfs
