#pragma once

#include <string>
#include <vector>

#include <glint/vfs/fwd.hpp>

namespace glint::vfs {

class IFileSystem {
  public:
    IFileSystem() = default;
    IFileSystem(const IFileSystem&) = delete;
    IFileSystem(IFileSystem&&) = delete;
    auto operator=(const IFileSystem&) -> IFileSystem& = delete;
    auto operator=(IFileSystem&&) -> IFileSystem& = delete;
    virtual ~IFileSystem() = default;

    virtual auto list(const std::string& path) -> std::vector<std::string> = 0;
    virtual auto exists(const std::string& path) -> bool = 0;
    virtual auto read(const std::string& path) -> std::vector<char> = 0;
    virtual auto read_str(const std::string& path) -> std::string = 0;
    virtual auto real_path(const std::string& path) -> std::string = 0;
};

} // namespace glint::vfs
