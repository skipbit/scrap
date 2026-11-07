#include "toolchain/MacSdk.h"

#include "process/Subprocess.h"

#include <chrono>
#include <cstddef>
#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace scrap::Toolchain {

namespace {

/// How long xcrun may take; it looks the SDK up rather than build anything.
constexpr std::chrono::seconds XcrunTimeout{ 60 };

/// The most xcrun is read for; a path is far shorter.
constexpr std::size_t XcrunOutputLimit = 4096;

}  // anonymous namespace

std::optional<std::filesystem::path> askXcrunForSdkPath()
{
    const std::vector<std::string> arguments{ "/usr/bin/xcrun", "--show-sdk-path" };
    const auto completion = Process::runProgram(arguments, { .workingDirectory = {}, .capture = Process::OutputCapture::StandardOutput, .group = Process::ProcessGroup::Caller, .timeout = XcrunTimeout, .outputLimit = XcrunOutputLimit });
    if ((! completion.has_value()) || (completion->exitCode != 0)) {
        return std::nullopt;
    }
    std::string answer = completion->output;
    while ((! answer.empty()) && ((answer.back() == '\n') || (answer.back() == '\r'))) {
        answer.pop_back();
    }
    if (answer.empty()) {
        return std::nullopt;
    }
    return std::filesystem::path{ answer };
}

std::expected<std::vector<std::string>, SdkNotFound> sdkArguments(std::string_view platform, const std::string& sdkRoot, const SdkPathQuery& query)
{
    if ((platform != "darwin") || (! sdkRoot.empty())) {
        return std::vector<std::string>{};
    }
    const std::optional<std::filesystem::path> sdk = query();
    if (! sdk.has_value()) {
        return std::unexpected(SdkNotFound{});
    }
    return std::vector<std::string>{ "-isysroot", sdk->string() };
}

}  // namespace scrap::Toolchain
