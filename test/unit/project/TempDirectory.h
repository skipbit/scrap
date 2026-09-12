#pragma once

#include <filesystem>
#include <string_view>

namespace scrap::TestSupport {

/**
 * @brief A uniquely named directory under the system temp location.
 *
 * Created on construction and removed on destruction, so a test can lay out a
 * real project tree on disk without leaving anything behind.
 */
class TempDirectory {
public:
    TempDirectory();
    ~TempDirectory();

    TempDirectory(const TempDirectory&) = delete;
    TempDirectory& operator=(const TempDirectory&) = delete;
    TempDirectory(TempDirectory&&) = delete;
    TempDirectory& operator=(TempDirectory&&) = delete;

    /**
     * @brief Path of the directory itself.
     */
    [[nodiscard]] const std::filesystem::path& path() const;

    /**
     * @brief Write a file below the directory, creating parent directories.
     *
     * Reports a test failure if the file cannot be written, so a broken
     * fixture is not mistaken for a failure of the code under test.
     *
     * @param relative Path relative to this directory.
     * @param content Bytes to write.
     * @return Full path of the written file.
     */
    std::filesystem::path writeFile(std::string_view relative, std::string_view content) const;

    /**
     * @brief Create a directory below this one.
     *
     * @param relative Path relative to this directory.
     * @return Full path of the created directory.
     */
    std::filesystem::path makeDirectory(std::string_view relative) const;

private:
    std::filesystem::path path_;
};

}  // namespace scrap::TestSupport
