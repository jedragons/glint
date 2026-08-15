#pragma once

#include <filesystem>

#include <glint/vfs/filesystem.hpp>
#include <glint/vfs/fwd.hpp>

namespace glint::vfs {

class NativeFileSystem final: public IFileSystem {
  public:
    NativeFileSystem() = delete;

    NativeFileSystem(std::filesystem::path point, std::filesystem::path root);

    auto list(const std::string& path) -> std::vector<std::string> override;
    auto exists(const std::string& path) -> bool override;
    auto read(const std::string& path) -> std::vector<char> override;
    auto read_str(const std::string& path) -> std::string override;
    auto real_path(const std::string& path) -> std::string override;

  private:
    std::filesystem::path m_point {};
    std::filesystem::path m_root {};

    [[nodiscard]]
    auto final_path(const std::string& path) const -> std::filesystem::path;
};

} // namespace glint::vfs
