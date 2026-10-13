#include "compile/CompilationDatabase.h"

#include "compile/CompileCommand.h"
#include "file/WholeFile.h"

#include <cstddef>
#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace scrap::Compile {

namespace {

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
    if (const std::error_code failure = File::replaceFile(file, renderCompilationDatabase(commands))) {
        return std::unexpected(DatabaseWriteFailure{ .step = DatabaseWriteStep::WriteFile, .path = file, .code = failure });
    }
    return {};
}

}  // namespace scrap::Compile
