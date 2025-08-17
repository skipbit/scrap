#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace scrap::test {

/**
 * @brief Helper class for file system operations in tests
 *
 * Provides utilities for creating temporary directories, files,
 * and cleaning up after tests. Follows RAII pattern for automatic cleanup.
 */
class FileSystemHelper {
public:
    /**
     * @brief RAII wrapper for temporary directory
     */
    class TempDirectory {
    private:
        std::filesystem::path path_;
        bool shouldCleanup_;

    public:
        explicit TempDirectory(const std::string& prefix = "scrap_test_");
        ~TempDirectory();

        // Disable copy
        TempDirectory(const TempDirectory&) = delete;
        TempDirectory& operator=(const TempDirectory&) = delete;

        // Enable move
        TempDirectory(TempDirectory&& other) noexcept;
        TempDirectory& operator=(TempDirectory&& other) noexcept;

        [[nodiscard]] const std::filesystem::path& path() const noexcept
        {
            return path_;
        }
        [[nodiscard]] operator std::filesystem::path() const
        {
            return path_;
        }

        void keepOnExit() noexcept
        {
            shouldCleanup_ = false;
        }
    };

    /**
     * @brief Create a temporary directory
     * @param prefix Prefix for the directory name
     * @return RAII wrapper that auto-cleans on destruction
     */
    [[nodiscard]] static TempDirectory createTempDirectory(const std::string& prefix = "scrap_test_");

    /**
     * @brief Create a file with content
     * @param path File path
     * @param content File content
     */
    static void createFile(const std::filesystem::path& path, const std::string& content);

    /**
     * @brief Create directory structure
     * @param path Directory path
     */
    static void createDirectories(const std::filesystem::path& path);

    /**
     * @brief Read file content
     * @param path File path
     * @return File content as string
     */
    [[nodiscard]] static std::string readFile(const std::filesystem::path& path);

    /**
     * @brief Check if file exists
     * @param path File path
     * @return true if exists
     */
    [[nodiscard]] static bool exists(const std::filesystem::path& path);

    /**
     * @brief Remove file or directory recursively
     * @param path Path to remove
     */
    static void remove(const std::filesystem::path& path);

    /**
     * @brief List files in directory
     * @param path Directory path
     * @param recursive Whether to list recursively
     * @return List of file paths
     */
    [[nodiscard]] static std::vector<std::filesystem::path> listFiles(const std::filesystem::path& path, bool recursive = false);

    /**
     * @brief Copy directory recursively
     * @param from Source directory
     * @param to Destination directory
     */
    static void copyDirectory(const std::filesystem::path& from, const std::filesystem::path& to);
};

}  // namespace scrap::test
