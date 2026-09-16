#pragma once

#include "project/ProjectFileSystem.h"

#include <expected>
#include <filesystem>
#include <string_view>
#include <system_error>

namespace scrap::Project {

/**
 * @brief ProjectFileSystem backed by the file system of the running system.
 */
class DiskProjectFileSystem : public ProjectFileSystem {
public:
    [[nodiscard]] auto absolute(const std::filesystem::path& path) const
        -> std::expected<std::filesystem::path, std::error_code> override;

    [[nodiscard]] auto createDirectory(const std::filesystem::path& directory) -> std::error_code override;

    [[nodiscard]] auto createDirectories(const std::filesystem::path& directory) -> std::error_code override;

    [[nodiscard]] auto writeNewFile(const std::filesystem::path& file,
                                    std::string_view content) -> std::error_code override;

    [[nodiscard]] auto removeAll(const std::filesystem::path& directory) -> std::error_code override;
};

}  // namespace scrap::Project
