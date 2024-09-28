#include "path.h"
#include "environment.h"

namespace scrap {

std::expected<path, std::filesystem::filesystem_error> path::mkdir(const std::string& absolute_path)
{
    return path::mkdir(std::filesystem::path{absolute_path});
}

std::expected<path, std::filesystem::filesystem_error> path::mkdir(const std::filesystem::path& absolute_path)
{
    try {
        std::error_code err;
        if (! std::filesystem::create_directories(absolute_path, err)) {
            return std::unexpected(std::filesystem::filesystem_error("failled", absolute_path, err));
        }
    } catch (const std::filesystem::filesystem_error& e) {
        return std::unexpected(e);
    }

    return path{absolute_path};
}

std::string path::separator()
{
    return "/";
}

path::path()
    : _path(std::filesystem::absolute(std::filesystem::path{"."}))
{
}

path::path(const std::string& p)
    : _path(p)
{
}

path::path(const std::filesystem::path& p)
    : _path(p)
{
}

path::path(const path& p)
    : _path(p._path)
{
}

path path::append(const std::string& component) const
{
    std::filesystem::path p(_path);
    if (component.substr(0, 1) == path::separator()) {
        p.append(std::string(component).replace(0, 1, ""));
    } else {
        p.append(component);
    }
    return path{p};
}

std::string path::string() const
{
    return _path.string();
}

std::expected<path, std::filesystem::filesystem_error> path::expand() const
{
    if (_path.string().substr(0, 1) == "~") {
        const auto expanded = environment::value("HOME").and_then([&](const std::string& env) {
            return std::make_optional(path{env}.append(_path.string().replace(0, 1, "")).string());
        });
        if (expanded) {
            return path { std::filesystem::canonical(std::filesystem::path{expanded.value()}) };
        } else {
            return std::unexpected(std::filesystem::filesystem_error("fail to expand tilde", _path, std::make_error_code(std::errc::no_such_file_or_directory)));
        }
    }

    return path{_path};
}

std::expected<path, std::filesystem::filesystem_error> path::resolve() const
{
    try {
        const auto expanded = expand();
        if (expanded) {
            return path { std::filesystem::canonical(expanded.value()) };
        } else {
            return expanded;
        }
    } catch (const std::filesystem::filesystem_error& e) {
        return std::unexpected(e);
    }
}

}
