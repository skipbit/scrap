#include "command/FetchFailureHint.h"

#include "command/WriteFailureHint.h"
#include "fetch/UrlFetch.h"

#include <string>
#include <string_view>
#include <system_error>
#include <variant>

namespace scrap::Command {

namespace {

constexpr std::string_view NetworkHint = "hint: check the network connection and run the command again\n";
constexpr std::string_view CertificateHint = "hint: install the root certificates of the system, such as the ca-certificates package, or set SSL_CERT_FILE to a file holding them\n";
constexpr std::string_view OneFileSystemHint = "hint: keep everything below the toolchains directory on one file system\n";

std::string hintFor(const Fetch::UrlRefused&)
{
    return std::string{ RetryHint };
}

std::string hintFor(const Fetch::NoCertificateBundle&)
{
    return std::string{ CertificateHint };
}

std::string hintFor(const Fetch::CertificateFailure&)
{
    return std::string{ CertificateHint };
}

std::string hintFor(const Fetch::TransferFailure&)
{
    return std::string{ NetworkHint };
}

std::string hintFor(const Fetch::UnexpectedStatus&)
{
    return std::string{ NetworkHint };
}

std::string hintFor(const Fetch::TooManyRedirects&)
{
    return std::string{ NetworkHint };
}

std::string hintFor(const Fetch::ContentTooLarge&)
{
    return std::string{ RetryHint };
}

std::string hintFor(const Fetch::CannotWriteDestination& error)
{
    return hintForWriteFailure(error.code);
}

}  // anonymous namespace

std::string hintForWriteFailure(const std::error_code& code)
{
    if (code == std::errc::cross_device_link) {
        return std::string{ OneFileSystemHint };
    }
    return std::string{ sharedWriteHint(code).value_or(RetryHint) };
}

std::string hintForFetchError(const Fetch::FetchError& error)
{
    return std::visit([](const auto& alternative) -> std::string {
        return hintFor(alternative);
    }, error);
}

}  // namespace scrap::Command
