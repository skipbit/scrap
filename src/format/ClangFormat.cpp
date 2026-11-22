#include "format/ClangFormat.h"

#include "file/WholeFile.h"
#include "process/Subprocess.h"
#include "toolchain/ProgramSearch.h"

#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace scrap::Format {

namespace {

/// The name clang-format is looked for by.
constexpr std::string_view ProgramName = "clang-format";

/// The word that comes before the version in what --version writes.
constexpr std::string_view VersionWord = "version ";

/**
 * @p path as it goes on the command line: a relative path that starts with
 * '-' or '@' gains a leading ./, so clang-format reads it as a file rather
 * than as an option or a file of options.
 */
std::string asArgument(const std::filesystem::path& path)
{
    if (path.is_relative() && (path.native().starts_with('-') || path.native().starts_with('@'))) {
        return (std::filesystem::path{ "." } / path).string();
    }
    return path.string();
}

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
    arguments.push_back(asArgument(file));
    return arguments;
}

/**
 * Run @p arguments in @p directory, reading back what @p capture names.
 */
std::expected<Process::Completion, ToolFailure> run(const std::vector<std::string>& arguments,
                                                    const std::filesystem::path& directory,
                                                    Process::OutputCapture capture)
{
    auto completion = Process::runProgram(arguments,
                                          { .workingDirectory = directory, .capture = capture, .group = Process::ProcessGroup::Caller, .timeout = std::nullopt, .outputLimit = std::nullopt });
    if (! completion.has_value()) {
        return std::unexpected(ToolNotRun{ .reason = completion.error().message() });
    }
    return std::move(*completion);
}

/// Whether @p completion is a run that succeeded.
bool succeeded(const Process::Completion& completion)
{
    return completion.exitCode == 0;
}

/// @p completion, which did not succeed, as a failure.
ToolFailed failureOf(Process::Completion completion)
{
    return ToolFailed{ .exitCode = completion.exitCode, .signal = completion.signal, .output = std::move(completion.output) };
}

/// The version in @p output, the text --version wrote.
std::string versionIn(std::string_view output)
{
    const std::string_view line = output.substr(0, output.find('\n'));
    const std::size_t word = line.find(VersionWord);
    if (word == std::string_view::npos) {
        return std::string{ line };
    }
    const std::string_view rest = line.substr(word + VersionWord.size());
    return std::string{ rest.substr(0, rest.find(' ')) };
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
    auto completion = run({ program.string(), "--version" }, {}, Process::OutputCapture::StandardOutput);
    if (! completion.has_value()) {
        return std::unexpected(completion.error());
    }
    if (! succeeded(*completion)) {
        return std::unexpected(failureOf(std::move(*completion)));
    }
    return ClangFormat{ .program = program, .version = versionIn(completion->output) };
}

std::expected<void, ToolFailure> formatFile(const ClangFormat& tool,
                                            const std::filesystem::path& projectRoot,
                                            const std::filesystem::path& file,
                                            const std::optional<std::string>& style)
{
    auto completion = run(commandFor(tool, { "-i" }, file, style), projectRoot, Process::OutputCapture::Combined);
    if (! completion.has_value()) {
        return std::unexpected(completion.error());
    }
    if (! succeeded(*completion)) {
        return std::unexpected(failureOf(std::move(*completion)));
    }
    return {};
}

std::expected<FileState, ToolFailure> checkFile(const ClangFormat& tool,
                                                const std::filesystem::path& projectRoot,
                                                const std::filesystem::path& file,
                                                const std::optional<std::string>& style)
{
    const std::vector<std::string> arguments = commandFor(tool, {}, file, style);
    auto formatted = run(arguments, projectRoot, Process::OutputCapture::StandardOutput);
    if (! formatted.has_value()) {
        return std::unexpected(formatted.error());
    }
    if (! succeeded(*formatted)) {
        // Standard error, which holds why, was not read with the formatted
        // text; a second run reads both.
        auto again = run(arguments, projectRoot, Process::OutputCapture::Combined);
        if (again.has_value() && (! succeeded(*again))) {
            return std::unexpected(failureOf(std::move(*again)));
        }
        return std::unexpected(failureOf(std::move(*formatted)));
    }
    const auto content = File::readWholeFile(projectRoot / file);
    if (! content.has_value()) {
        return std::unexpected(FileUnreadable{});
    }
    return (*content == formatted->output) ? FileState::Formatted : FileState::Unformatted;
}

}  // namespace scrap::Format
