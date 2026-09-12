#pragma once

#include <atomic>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <string_view>
#include <system_error>

namespace scrap::TestSupport {

/**
 * @brief A uniquely named directory under the system temp location.
 *
 * Created on construction and removed on destruction, so a test can lay out a
 * real project tree on disk without leaving anything behind.
 */
class TempDirectory {
public:
    TempDirectory()
    {
        static std::atomic<unsigned> counter{0};
        std::random_device device;
        const std::string unique = std::to_string(device()) + "-" + std::to_string(counter.fetch_add(1));
        path_ = std::filesystem::temp_directory_path() / ("scrap-project-test-" + unique);
        std::filesystem::create_directories(path_);
    }

    TempDirectory(const TempDirectory&) = delete;
    TempDirectory& operator=(const TempDirectory&) = delete;
    TempDirectory(TempDirectory&&) = delete;
    TempDirectory& operator=(TempDirectory&&) = delete;

    ~TempDirectory()
    {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }

    /**
     * @brief Path of the directory itself.
     */
    [[nodiscard]] const std::filesystem::path& path() const
    {
        return path_;
    }

    /**
     * @brief Write a file below the directory, creating parent directories.
     *
     * @param relative Path relative to this directory.
     * @param content Bytes to write.
     * @return Full path of the written file.
     */
    std::filesystem::path writeFile(std::string_view relative, std::string_view content) const
    {
        const std::filesystem::path target = path_ / relative;
        std::filesystem::create_directories(target.parent_path());
        std::ofstream output(target, std::ios::binary);
        output << content;
        return target;
    }

    /**
     * @brief Create a directory below this one.
     *
     * @param relative Path relative to this directory.
     * @return Full path of the created directory.
     */
    std::filesystem::path makeDirectory(std::string_view relative) const
    {
        const std::filesystem::path target = path_ / relative;
        std::filesystem::create_directories(target);
        return target;
    }

private:
    std::filesystem::path path_;
};

}  // namespace scrap::TestSupport
