#pragma once

#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace scrap::Toolchain {

/**
 * @brief Where the macOS SDK is, or nothing when it cannot be told.
 */
using SdkPathQuery = std::function<std::optional<std::filesystem::path>()>;

/**
 * @brief Ask xcrun --show-sdk-path where the macOS SDK is.
 *
 * @return The path it prints, without the line end, or nothing when it
 *   cannot be run, fails or prints nothing.
 */
[[nodiscard]] std::optional<std::filesystem::path> askXcrunForSdkPath();

/**
 * @brief The macOS SDK could not be found.
 */
struct SdkNotFound { };

/**
 * @brief What a compiler is given to compile and link against the macOS SDK
 *        on @p platform.
 *
 * The compiler of the system finds the SDK itself, so it is given nothing.
 * The compiler of an installed toolchain does not: on darwin it is given
 * -isysroot and the path @p query tells, unless @p sdkRoot names an SDK,
 * which the compiler then reads itself. Elsewhere it is given nothing.
 * @p query is asked only when it is needed.
 *
 * @param findsSdkItself Whether the compiler finds the SDK without help.
 * @param platform The platform of the host, as the index spells it.
 * @param sdkRoot The SDK the environment names in SDKROOT, or empty.
 * @param query Where to ask for the SDK.
 */
[[nodiscard]] std::expected<std::vector<std::string>, SdkNotFound> sdkArguments(bool findsSdkItself, std::string_view platform, const std::string& sdkRoot, const SdkPathQuery& query);

}  // namespace scrap::Toolchain
