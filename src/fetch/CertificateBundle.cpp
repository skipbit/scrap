#include "fetch/CertificateBundle.h"

#include <array>
#include <filesystem>
#include <optional>
#include <span>
#include <string_view>
#include <system_error>

namespace scrap::Fetch {

namespace {

constexpr std::array<std::string_view, 5> KnownBundles = {
    "/etc/ssl/certs/ca-certificates.crt",                 // Debian, Ubuntu, Arch, Alpine
    "/etc/pki/tls/certs/ca-bundle.crt",                   // Fedora, RHEL, Rocky
    "/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem",  // Fedora, RHEL, Rocky
    "/etc/ssl/ca-bundle.pem",                             // openSUSE
    "/etc/ssl/cert.pem",                                  // macOS, the BSDs
};

}  // anonymous namespace

std::span<const std::string_view> knownCertificateBundles()
{
    return KnownBundles;
}

std::optional<std::filesystem::path> findCertificateBundle(const std::string_view requested, const std::span<const std::string_view> known)
{
    if (! requested.empty()) {
        return std::filesystem::path{ requested };
    }
    for (const std::string_view candidate : known) {
        std::error_code ec;
        if (std::filesystem::is_regular_file(candidate, ec)) {
            return std::filesystem::path{ candidate };
        }
    }
    return std::nullopt;
}

}  // namespace scrap::Fetch
