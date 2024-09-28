#pragma once

#include <expected>
#include <filesystem>

namespace scrap {

class path {
public:
    static std::expected<path, std::filesystem::filesystem_error> mkdir(const std::string&);
    static std::expected<path, std::filesystem::filesystem_error> mkdir(const std::filesystem::path&);
    static std::string separator();

    path();
    path(const std::string&);
    path(const std::filesystem::path&);
    path(const path&);

    path append(const std::string&) const;
    std::string string() const;

    std::expected<path, std::filesystem::filesystem_error> expand() const;
    std::expected<path, std::filesystem::filesystem_error> resolve() const;
private:
    std::filesystem::path _path;
};

}
