#pragma once

#include <expected>
#include <filesystem>
#include <string_view>
#include <system_error>

namespace scrap::Project {

/**
 * @brief The file operations creating a project needs.
 *
 * Creating a project reaches the file system only through these operations,
 * so a test can stand in for a file system that refuses them. The
 * implementation for the real file system lives in driver/.
 */
class ProjectFileSystem {
public:
    virtual ~ProjectFileSystem();
    ProjectFileSystem(const ProjectFileSystem&) = default;
    ProjectFileSystem& operator=(const ProjectFileSystem&) = default;
    ProjectFileSystem(ProjectFileSystem&&) = default;
    ProjectFileSystem& operator=(ProjectFileSystem&&) = default;

    /**
     * @brief Resolve @p path against the process's current directory.
     *
     * A caller that carries a working directory of its own passes an absolute
     * path, which this returns unchanged; the process's directory is the base
     * for anything relative.
     *
     * @param path Path to resolve.
     * @return The absolute path, or the failure the system reported.
     */
    [[nodiscard]] virtual std::expected<std::filesystem::path, std::error_code> absolute(const std::filesystem::path& path) const
        = 0;

    /**
     * @brief Create @p directory alone, and only where nothing exists yet.
     *
     * @param directory Directory to create.
     * @return An empty code on success, std::errc::file_exists when anything
     *         is already at that path, or the failure the system reported.
     */
    [[nodiscard]] virtual std::error_code createDirectory(const std::filesystem::path& directory) = 0;

    /**
     * @brief Create @p directory together with any missing parent.
     *
     * @param directory Directory to create.
     * @return An empty code on success, or the failure the system reported.
     */
    [[nodiscard]] virtual std::error_code createDirectories(const std::filesystem::path& directory) = 0;

    /**
     * @brief Write @p content to @p file, which must not exist yet.
     *
     * @param file File to create.
     * @param content Bytes to write.
     * @return An empty code on success, or the failure the system reported.
     */
    [[nodiscard]] virtual std::error_code writeNewFile(const std::filesystem::path& file, std::string_view content) = 0;

    /**
     * @brief Remove @p directory and everything below it.
     *
     * @param directory Directory to remove.
     * @return An empty code on success, or the failure the system reported.
     */
    [[nodiscard]] virtual std::error_code removeAll(const std::filesystem::path& directory) = 0;

protected:
    ProjectFileSystem() = default;
};

}  // namespace scrap::Project
