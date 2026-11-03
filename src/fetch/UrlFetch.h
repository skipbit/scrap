#pragma once

#include <chrono>
#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <variant>

namespace scrap::Fetch {

/**
 * @brief What to fetch, and where to put it.
 */
struct FetchRequest {
    std::string url;
    std::filesystem::path destination;

    /// Root certificates to verify the host against, and a proxy reached over
    /// https. An https URL needs them. On Linux they are all that is trusted; the
    /// libcurl of macOS adds them to what the OS trusts.
    std::optional<std::filesystem::path> certificateBundle;

    /// How long nothing may arrive before the transfer is given up.
    std::chrono::seconds stallLimit = std::chrono::seconds{ 60 };
};

/**
 * @brief The URL, or one a redirect led to, is neither https nor http to this
 *        machine, or is http reached by a redirect from elsewhere.
 */
struct UrlRefused {
    std::string url;
};

/**
 * @brief An https URL was to be fetched with no root certificates to verify it.
 */
struct NoCertificateBundle {
    std::string url;
};

/**
 * @brief The host's certificate did not verify, or the root certificates could
 *        not be read.
 */
struct CertificateFailure {
    std::string url;
    std::string message;
};

/**
 * @brief The transfer did not complete.
 */
struct TransferFailure {
    std::string url;
    std::string message;
};

/**
 * @brief The server answered with something other than the content.
 */
struct UnexpectedStatus {
    std::string url;
    long status = 0;
};

/**
 * @brief Redirects went on past the limit.
 */
struct TooManyRedirects {
    std::string url;
};

/**
 * @brief The destination could not be written.
 */
struct CannotWriteDestination {
    std::filesystem::path path;
    std::error_code code;
};

using FetchError = std::variant<UrlRefused, NoCertificateBundle, CertificateFailure, TransferFailure, UnexpectedStatus, TooManyRedirects, CannotWriteDestination>;

/**
 * @brief Fetch a URL into a file, following redirects.
 *
 * Only https is accepted, and http to this machine (127.0.0.1, ::1, localhost);
 * the same goes for each redirect, which reaches http only from this machine. A
 * proxy named by the usual environment variables is used, except for this
 * machine.
 *
 * The destination changes only when the response has ended as it said it would.
 * One that ends by closing the connection cannot show it was cut short, so a
 * caller that must have the whole content checks it.
 *
 * @return Nothing on success, or what went wrong.
 */
[[nodiscard]] std::expected<void, FetchError> fetchToFile(const FetchRequest& request);

}  // namespace scrap::Fetch
