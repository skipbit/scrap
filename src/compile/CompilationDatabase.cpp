#include "compile/CompilationDatabase.h"

#include "compile/CompileCommand.h"

#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <expected>  // IWYU pragma: keep
#include <fcntl.h>
#include <filesystem>
#include <string>
#include <string_view>
#include <sys/types.h>
#include <system_error>
#include <unistd.h>
#include <vector>

namespace scrap::Compile {

namespace {

/// Permissions the database is created with, before the process umask.
constexpr mode_t DatabaseFileMode = 0644;

/// How many staged names are tried when others are already taken.
constexpr int StagingAttempts = 16;

/**
 * Append @p text to @p out as a JSON string. A quote, a backslash and the
 * control characters are escaped; every other byte is copied as it is.
 */
void appendJsonString(std::string& out, std::string_view text)
{
    static constexpr std::string_view HexDigits = "0123456789abcdef";
    out += '"';
    for (const char ch : text) {
        const unsigned byte = static_cast<unsigned char>(ch);
        if ((ch == '"') || (ch == '\\')) {
            out += '\\';
            out += ch;
        } else if (byte < 0x20U) {
            out += "\\u00";
            out += HexDigits[byte >> 4U];
            out += HexDigits[byte & 0x0FU];
        } else {
            out += ch;
        }
    }
    out += '"';
}

void appendEntry(std::string& out, const CompileCommand& command)
{
    out += "  {\n    \"directory\": ";
    appendJsonString(out, command.directory.string());
    out += ",\n    \"file\": ";
    appendJsonString(out, command.file.string());
    out += ",\n    \"arguments\": [";
    for (std::size_t index = 0; index < command.arguments.size(); ++index) {
        if (index > 0) {
            out += ", ";
        }
        appendJsonString(out, command.arguments[index]);
    }
    out += "],\n    \"output\": ";
    appendJsonString(out, command.output.string());
    out += "\n  }";
}

/**
 * The failure the last system call reported.
 */
std::error_code lastFailure()
{
    return { errno, std::generic_category() };
}

/**
 * Write the whole of @p content to @p descriptor.
 */
std::error_code writeAll(const int descriptor, std::string_view content)
{
    const char* data = content.data();
    std::size_t remaining = content.size();
    while (remaining > 0) {
        const ssize_t written = ::write(descriptor, data, remaining);
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            return lastFailure();
        }
        if (written == 0) {
            // POSIX allows a write of zero bytes to report nothing written,
            // which would leave the loop where it started.
            return std::make_error_code(std::errc::io_error);
        }
        data += written;
        remaining -= static_cast<std::size_t>(written);
    }
    return {};
}

/**
 * Put @p content at @p file by writing it beside the file under a name no
 * other file holds, then renaming it over the file. The name carries this
 * process's id and a count; creating it exclusively moves on to the next
 * count when another build, one in another container with the same id among
 * them, already holds it. The staged file is removed when a later step fails.
 */
std::error_code replaceFile(const std::filesystem::path& file, std::string_view content)
{
    const std::string prefix = file.string() + "." + std::to_string(::getpid()) + ".";
    for (int attempt = 0; attempt < StagingAttempts; ++attempt) {
        const std::filesystem::path staged = prefix + std::to_string(attempt) + ".tmp";
        // NOLINTNEXTLINE(hicpp-signed-bitwise) - POSIX open() flag combination
        const int descriptor = ::open(staged.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, DatabaseFileMode);
        if (descriptor < 0) {
            if (errno == EEXIST) {
                continue;
            }
            return lastFailure();
        }

        std::error_code failure = writeAll(descriptor, content);
        if ((::close(descriptor) != 0) && (! failure)) {
            failure = lastFailure();
        }
        if ((! failure) && (::rename(staged.c_str(), file.c_str()) != 0)) {
            failure = lastFailure();
        }
        if (failure) {
            ::unlink(staged.c_str());
        }
        return failure;
    }
    return std::make_error_code(std::errc::file_exists);
}

}  // anonymous namespace

std::string renderCompilationDatabase(const std::vector<CompileCommand>& commands)
{
    if (commands.empty()) {
        return "[]\n";
    }

    std::string out = "[\n";
    for (std::size_t index = 0; index < commands.size(); ++index) {
        if (index > 0) {
            out += ",\n";
        }
        appendEntry(out, commands[index]);
    }
    out += "\n]\n";
    return out;
}

std::expected<void, DatabaseWriteFailure> writeCompilationDatabase(const std::filesystem::path& buildDirectory,
                                                                   const std::vector<CompileCommand>& commands)
{
    std::error_code ec;
    std::filesystem::create_directories(buildDirectory, ec);
    if (ec) {
        return std::unexpected(DatabaseWriteFailure{ .step = DatabaseWriteStep::CreateDirectory, .path = buildDirectory, .code = ec });
    }

    const std::filesystem::path file = buildDirectory / CompilationDatabaseFileName;
    if (const std::error_code failure = replaceFile(file, renderCompilationDatabase(commands))) {
        return std::unexpected(DatabaseWriteFailure{ .step = DatabaseWriteStep::WriteFile, .path = file, .code = failure });
    }
    return {};
}

}  // namespace scrap::Compile
