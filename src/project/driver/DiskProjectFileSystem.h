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
    [[nodiscard]] std::expected<std::filesystem::path, std::error_code> absolute(const std::filesystem::path& path) const override;

    [[nodiscard]] std::error_code createDirectory(const std::filesystem::path& directory) override;

    [[nodiscard]] std::error_code createDirectories(const std::filesystem::path& directory) override;

    [[nodiscard]] std::error_code writeNewFile(const std::filesystem::path& file, std::string_view content) override;

    [[nodiscard]] std::error_code removeAll(const std::filesystem::path& directory) override;
};

}  // namespace scrap::Project
