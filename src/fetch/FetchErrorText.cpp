#include "fetch/FetchErrorText.h"

#include "fetch/UrlFetch.h"

#include <string>
#include <variant>

namespace scrap::Fetch {

namespace {

std::string describe(const UrlRefused& error)
{
    return "only https is accepted, but got '" + error.url + "'";
}

std::string describe(const NoCertificateBundle&)
{
    return "no root certificates were found to verify the host";
}

std::string describe(const CertificateFailure& error)
{
    return "cannot verify '" + error.url + "': " + error.message;
}

std::string describe(const TransferFailure& error)
{
    return "'" + error.url + "': " + error.message;
}

std::string describe(const UnexpectedStatus& error)
{
    return "'" + error.url + "' answered with status " + std::to_string(error.status);
}

std::string describe(const TooManyRedirects& error)
{
    return "too many redirects, the last to '" + error.url + "'";
}

std::string describe(const ContentTooLarge& error)
{
    return "'" + error.url + "' sent more than " + std::to_string(error.limit) + " bytes";
}

std::string describe(const CannotWriteDestination& error)
{
    return "cannot write '" + error.path.string() + "': " + error.code.message();
}

}  // anonymous namespace

std::string describeFetchError(const FetchError& error)
{
    return std::visit([](const auto& alternative) -> std::string {
        return describe(alternative);
    }, error);
}

}  // namespace scrap::Fetch
