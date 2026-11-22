#include "format/ClangFormat.h"

#include "file/WholeFile.h"
#include "process/Subprocess.h"
#include "toolchain/CommandArgument.h"
#include "toolchain/ProgramSearch.h"

#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace scrap::Format {

namespace {

/// The name clang-format is looked for by.
constexpr std::string_view ProgramName = "clang-format";

/// The word that comes before the version in what --version writes.
constexpr std::string_view VersionWord = "version ";

/// The version shown when --version writes nothing to show.
constexpr std::string_view UnknownVersion = "unknown";

/**
 * The command line that runs @p tool on @p file, after @p options.
 */
std::vector<std::string> commandFor(const ClangFormat& tool,
                                    const std::vector<std::string>& options,
                                    const std::filesystem::path& file,
                                    const std::optional<std::string>& style)
{
    std::vector<std::string> arguments{ tool.program.string() };
    arguments.insert(arguments.end(), options.begin(), options.end());
    if (style.has_value()) {
        arguments.push_back("--style=" + *style);
    }
    arguments.push_back(Toolchain::pathAsArgument(file).string());
    return arguments;
}

/**
 * Run @p arguments in @p directory, reading back what @p capture names, and
 * return what was read when it succeeded.
 */
std::expected<std::string, ToolFailure> runToSuccess(const std::vector<std::string>& arguments,
                                                     const std::filesystem::path& directory,
                                                     Process::OutputCapture capture)
{
    auto completion = Process::runProgram(arguments,
                                          { .workingDirectory = directory, .capture = capture, .group = Process::ProcessGroup::Caller, .timeout = std::nullopt, .outputLimit = std::nullopt });
    if (! completion.has_value()) {
        return std::unexpected(ToolNotRun{ .code = completion.error() });
    }
    if (completion->exitCode != 0) {
        return std::unexpected(ToolFailed{ .exitCode = completion->exitCode, .signal = completion->signal, .output = std::move(completion->output) });
    }
    return std::move(completion->output);
}

/**
 * Run @p arguments in @p directory and return what it wrote to standard
 * output. Process::runProgram cannot read the two streams apart, so when it
 * does not succeed a second run reads both, standard error holding why.
 */
std::expected<std::string, ToolFailure> standardOutputOf(const std::vector<std::string>& arguments, const std::filesystem::path& directory)
{
    auto output = runToSuccess(arguments, directory, Process::OutputCapture::StandardOutput);
    if (output.has_value() || std::holds_alternative<ToolNotRun>(output.error())) {
        return output;
    }
    const auto again = runToSuccess(arguments, directory, Process::OutputCapture::Combined);
    if (! again.has_value() && std::holds_alternative<ToolFailed>(again.error())) {
        return again;
    }
    return output;
}

/// The lines of @p text, without their newlines.
std::vector<std::string_view> linesOf(std::string_view text)
{
    std::vector<std::string_view> lines;
    while (! text.empty()) {
        const std::size_t end = text.find('\n');
        lines.push_back(text.substr(0, end));
        text = (end == std::string_view::npos) ? std::string_view{} : text.substr(end + 1);
    }
    return lines;
}

/**
 * The version in @p output, the text --version wrote: the word after
 * "version" on the first line that has one, else the first line that is not
 * empty, else "unknown".
 */
std::string versionIn(std::string_view output)
{
    const std::vector<std::string_view> lines = linesOf(output);
    for (const std::string_view line : lines) {
        if (const std::size_t word = line.find(VersionWord); word != std::string_view::npos) {
            const std::string_view rest = line.substr(word + VersionWord.size());
            return std::string{ rest.substr(0, rest.find(' ')) };
        }
    }
    for (const std::string_view line : lines) {
        if (! line.empty()) {
            return std::string{ line };
        }
    }
    return std::string{ UnknownVersion };
}

}  // anonymous namespace

std::optional<std::filesystem::path> findClangFormat(const std::vector<std::filesystem::path>& searchPaths)
{
    const auto found = Toolchain::findOnSearchPaths(ProgramName, searchPaths);
    if (! found.has_value()) {
        return std::nullopt;
    }
    return Toolchain::absoluteProgramPath(*found);
}

std::expected<ClangFormat, ToolFailure> describeClangFormat(const std::filesystem::path& program)
{
    const auto output = standardOutputOf({ program.string(), "--version" }, {});
    if (! output.has_value()) {
        return std::unexpected(output.error());
    }
    return ClangFormat{ .program = Toolchain::absoluteProgramPath(program), .version = versionIn(*output) };
}

std::expected<void, ToolFailure> formatFile(const ClangFormat& tool,
                                            const std::filesystem::path& projectRoot,
                                            const std::filesystem::path& file,
                                            const std::optional<std::string>& style)
{
    const auto output = runToSuccess(commandFor(tool, { "-i" }, file, style), projectRoot, Process::OutputCapture::Combined);
    if (! output.has_value()) {
        return std::unexpected(output.error());
    }
    return {};
}

std::expected<FileState, ToolFailure> checkFile(const ClangFormat& tool,
                                                const std::filesystem::path& projectRoot,
                                                const std::filesystem::path& file,
                                                const std::optional<std::string>& style)
{
    const auto formatted = standardOutputOf(commandFor(tool, {}, file, style), projectRoot);
    if (! formatted.has_value()) {
        return std::unexpected(formatted.error());
    }
    const auto content = File::readWholeFile(projectRoot / file);
    if (! content.has_value()) {
        return std::unexpected(FileUnreadable{});
    }
    return (*content == *formatted) ? FileState::Formatted : FileState::Unformatted;
}

}  // namespace scrap::Format
