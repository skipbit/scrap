#pragma once

#include <expected>
#include <filesystem>
#include <string_view>
#include <system_error>

namespace scrap::Install {

/**
 * @brief A work directory that could not be made.
 */
struct WorkDirectoryNotMade {
    std::filesystem::path path;
    std::error_code code;
};

/**
 * @brief A directory of its own for one run, removed however the run ends
 *        unless the run asks to keep part of it.
 */
class WorkDirectory {
public:
    /**
     * A new directory below @p parent. Its name starts with @p prefix and
     * holds this process's id and a count that moves on when another
     * process, one in another container with the same id among them, holds
     * the name.
     *
     * @return The directory, or what could not be made and why.
     */
    [[nodiscard]] static std::expected<WorkDirectory, WorkDirectoryNotMade> below(const std::filesystem::path& parent, std::string_view prefix);

    WorkDirectory(const WorkDirectory&) = delete;
    WorkDirectory& operator=(const WorkDirectory&) = delete;
    WorkDirectory(WorkDirectory&& other) noexcept;
    WorkDirectory& operator=(WorkDirectory&&) = delete;
    ~WorkDirectory();

    [[nodiscard]] const std::filesystem::path& path() const;

    /**
     * Leave @p kept behind when the run ends, and nothing else of the run.
     */
    void keepOnly(const std::filesystem::path& kept);

private:
    explicit WorkDirectory(std::filesystem::path path);

    std::filesystem::path _path;
};

/**
 * @brief Remove what stopped runs left in @p workDirectory, when older than
 *        @p now by a day.
 *
 * A younger one may belong to a run still going. Failures are passed over: this
 * only reclaims space.
 */
void discardAbandonedWork(const std::filesystem::path& workDirectory, std::filesystem::file_time_type now);

}  // namespace scrap::Install
