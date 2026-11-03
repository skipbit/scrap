#pragma once

#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <string_view>
#include <system_error>

namespace scrap::File {

/**
 * @brief A file written piece by piece beside its destination, which appears
 *        there whole or not at all.
 *
 * The staged file has a name no other file holds, so two processes writing at
 * once each write a file of their own. The name carries this process's id and
 * a count; creating it exclusively moves on to the next count when another
 * process, one in another container with the same id among them, already holds
 * it. It is removed unless it is committed. The file is created readable by
 * everyone and writable by its owner, before the process umask.
 */
class StagedFile {
public:
    /**
     * @brief Begin a file that will end up at @p destination.
     *
     * @return The staged file, or what the operating system reported.
     */
    [[nodiscard]] static std::expected<StagedFile, std::error_code> begin(const std::filesystem::path& destination);

    StagedFile(StagedFile&& other) noexcept;
    StagedFile& operator=(StagedFile&& other) noexcept;
    StagedFile(const StagedFile&) = delete;
    StagedFile& operator=(const StagedFile&) = delete;
    ~StagedFile();

    /**
     * @brief Add @p content to the end of the file.
     *
     * @return Nothing on success, or what the operating system reported.
     */
    [[nodiscard]] std::error_code append(std::string_view content) const;

    /**
     * @brief Rename the file over its destination.
     *
     * @return Nothing on success, bad_file_descriptor when already committed or
     *         moved from, or what the operating system reported.
     */
    [[nodiscard]] std::error_code commit();

private:
    StagedFile(std::filesystem::path destination, std::filesystem::path staged, int descriptor);

    void discard() noexcept;

    std::filesystem::path _destination;
    std::filesystem::path _staged;
    int _descriptor = -1;
};

}  // namespace scrap::File
