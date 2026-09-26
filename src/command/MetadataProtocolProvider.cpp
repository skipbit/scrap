#include "command/MetadataProtocolProvider.h"

#include "command/ExternalMetadataProvider.h"
#include "process/Subprocess.h"

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace scrap::Command {

namespace {

// --- Protocol constants -------------------------------------------------------

constexpr const char* ProtocolFlag = "--scrap-metadata";
constexpr const char* HelpFlag = "--help";

// Upper bound on captured stdout; a metadata description is a single short
// line, so this is generous headroom rather than an expected size.
constexpr std::size_t MaxCaptureBytes = (64UL * 1024UL);

/**
 * Extract the first non-empty, whitespace-trimmed line from @p text.
 *
 * Returns an empty string if every line is blank (or @p text is empty).
 */
std::string firstNonEmptyLine(std::string_view text)
{
    std::size_t pos = 0;
    while (pos <= text.size()) {
        auto newlinePos = text.find('\n', pos);
        auto lineEnd = (newlinePos == std::string_view::npos) ? text.size() : newlinePos;
        auto line = text.substr(pos, lineEnd - pos);

        while ((! line.empty()) && ((line.front() == ' ') || (line.front() == '\t'))) {
            line.remove_prefix(1);
        }
        while ((! line.empty()) && ((line.back() == ' ') || (line.back() == '\t') || (line.back() == '\r'))) {
            line.remove_suffix(1);
        }

        if (! line.empty()) {
            return std::string{ line };
        }
        if (newlinePos == std::string_view::npos) {
            break;
        }
        pos = newlinePos + 1;
    }
    return {};
}

/**
 * Run @p executable with @p flag and return its first-line description, or
 * an empty string if the attempt did not yield a usable result (non-zero
 * exit, empty output, spawn failure, or timeout).
 */
std::string probe(const std::filesystem::path& executable, const char* flag, std::chrono::milliseconds timeout)
{
    // A group of its own lets a hung probe be killed with everything it started.
    const auto completion = Process::runProgram({ executable.string(), flag },
                                                { .workingDirectory = {},
                                                  .capture = Process::OutputCapture::StandardOutput,
                                                  .group = Process::ProcessGroup::Own,
                                                  .timeout = timeout,
                                                  .outputLimit = MaxCaptureBytes });
    if ((! completion.has_value()) || completion->timedOut || (completion->exitCode != 0)) {
        return {};
    }
    return firstNonEmptyLine(completion->output);
}

}  // namespace

/**
 * Construct with the subprocess timeout used for both probe attempts.
 */
MetadataProtocolProvider::MetadataProtocolProvider(std::chrono::milliseconds timeout)
    : _timeout(timeout)
{
}

/**
 * Fetch metadata via --scrap-metadata, falling back to --help.
 *
 * The path is canonicalized so the program started is the file it resolves
 * to, with relative parts and symbolic links resolved; no shell is involved.
 * Only a plain-text first-line description is extracted; structured
 * (name/options) metadata is not yet part of the protocol.
 */
std::expected<ExternalCommandMetadata, std::string> MetadataProtocolProvider::fetch(const std::filesystem::path& executable)
{
    std::error_code ec;
    auto canonicalized = std::filesystem::weakly_canonical(executable, ec);
    const std::filesystem::path& exe = ec ? executable : canonicalized;

    if (auto description = probe(exe, ProtocolFlag, _timeout); ! description.empty()) {
        return ExternalCommandMetadata{ .name = "", .description = std::move(description), .options = {} };
    }

    if (auto description = probe(exe, HelpFlag, _timeout); ! description.empty()) {
        return ExternalCommandMetadata{ .name = "", .description = std::move(description), .options = {} };
    }

    return std::unexpected("no usable metadata from '" + exe.string() + "' via --scrap-metadata or --help");
}

}  // namespace scrap::Command
