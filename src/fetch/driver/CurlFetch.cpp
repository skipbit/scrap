#include "fetch/UrlFetch.h"
#include "fetch/UrlPolicy.h"
#include "file/StagedFile.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <curl/curl.h>
#include <curl/easy.h>
#include <curl/header.h>
#include <curl/urlapi.h>
#include <expected>  // IWYU pragma: keep
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace scrap::Fetch {

namespace {

constexpr int RedirectLimit = 10;
constexpr long ConnectTimeoutSeconds = 30;
constexpr long HttpOk = 200;
constexpr long HttpUnauthorized = 401;
constexpr long FirstRedirectStatus = 300;
constexpr long PastLastRedirectStatus = 400;

struct EasyHandleDeleter {
    void operator()(CURL* handle) const
    {
        curl_easy_cleanup(handle);
    }
};
using EasyHandle = std::unique_ptr<CURL, EasyHandleDeleter>;

struct UrlHandleDeleter {
    void operator()(CURLU* handle) const
    {
        curl_url_cleanup(handle);
    }
};
using UrlHandle = std::unique_ptr<CURLU, UrlHandleDeleter>;

struct UrlPartDeleter {
    void operator()(char* part) const
    {
        curl_free(part);
    }
};
using UrlPart = std::unique_ptr<char, UrlPartDeleter>;

struct HeaderListDeleter {
    void operator()(curl_slist* list) const
    {
        curl_slist_free_all(list);
    }
};
using HeaderList = std::unique_ptr<curl_slist, HeaderListDeleter>;

/**
 * @brief Where the body of a response goes, and what went wrong putting it there.
 */
struct BodySink {
    CURL* easy = nullptr;
    File::StagedFile* file = nullptr;
    std::optional<std::uint64_t> sizeLimit;
    std::uint64_t received = 0;
    bool tooLarge = false;
    std::error_code failure;
};

std::optional<std::string> urlPart(CURLU* url, const CURLUPart part, const unsigned int flags = 0)
{
    char* text = nullptr;
    if (curl_url_get(url, part, &text, flags) != CURLUE_OK) {
        return std::nullopt;
    }
    const UrlPart owned{ text };
    return std::string{ text };
}

/**
 * Read what a URL says about where it leads as libcurl itself reads it, so
 * the rule in UrlPolicy and the transfer cannot disagree.
 */
std::optional<UrlTarget> readTarget(const std::string& text)
{
    const UrlHandle url{ curl_url() };
    if ((url == nullptr) || (curl_url_set(url.get(), CURLUPART_URL, text.c_str(), 0) != CURLUE_OK)) {
        return std::nullopt;
    }
    auto scheme = urlPart(url.get(), CURLUPART_SCHEME);
    auto host = urlPart(url.get(), CURLUPART_HOST);
    auto port = urlPart(url.get(), CURLUPART_PORT, CURLU_DEFAULT_PORT);
    if ((! scheme.has_value()) || (! host.has_value()) || (! port.has_value())) {
        return std::nullopt;
    }
    return UrlTarget{ .scheme = std::move(*scheme), .host = std::move(*host), .port = std::move(*port) };
}

/**
 * Put the body of a successful response in the file. libcurl hands over the
 * bodies of all responses, and those of redirects and errors are nobody's.
 */
std::size_t receiveBody(const char* data, const std::size_t size, const std::size_t count, void* userData)
{
    auto* sink = static_cast<BodySink*>(userData);
    const std::size_t length = size * count;

    long status = 0;
    curl_easy_getinfo(sink->easy, CURLINFO_RESPONSE_CODE, &status);
    if (status != HttpOk) {
        return length;
    }

    sink->received += length;
    if (sink->sizeLimit.has_value() && (sink->received > *sink->sizeLimit)) {
        sink->tooLarge = true;
        return 0;
    }
    sink->failure = sink->file->append(std::string_view{ data, length });
    return sink->failure ? 0 : length;
}

/**
 * The header lines of @p request as libcurl takes them, or nothing when there
 * are none or they cannot be put together.
 */
std::expected<HeaderList, FetchError> headerList(const FetchRequest& request)
{
    HeaderList list;
    for (const std::string& line : request.headers) {
        curl_slist* longer = curl_slist_append(list.get(), line.c_str());
        if (longer == nullptr) {
            return std::unexpected(TransferFailure{ .url = request.url, .message = "cannot set up the request headers" });
        }
        // Appending returns the head of the list, which is new only for the first line.
        if (list == nullptr) {
            list.reset(longer);
        }
    }
    return list;
}

/**
 * What a 401 answer asks for, so a caller can authenticate and ask again.
 */
std::string authenticationAsked(CURL* easy)
{
    curl_header* header = nullptr;
    if (curl_easy_header(easy, "WWW-Authenticate", 0, CURLH_HEADER, -1, &header) != CURLHE_OK) {
        return {};
    }
    return std::string{ header->value };
}

void applyOptions(CURL* easy, const FetchRequest& request, const std::string& url, const UrlTarget& target, BodySink& sink, char* errorText)
{
    curl_easy_reset(easy);
    curl_easy_setopt(easy, CURLOPT_URL, url.c_str());
    curl_easy_setopt(easy, CURLOPT_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(easy, CURLOPT_USERAGENT, "scrap");
    curl_easy_setopt(easy, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(easy, CURLOPT_ERRORBUFFER, errorText);
    curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, &receiveBody);
    curl_easy_setopt(easy, CURLOPT_WRITEDATA, &sink);
    curl_easy_setopt(easy, CURLOPT_CONNECTTIMEOUT, ConnectTimeoutSeconds);
    curl_easy_setopt(easy, CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(easy, CURLOPT_LOW_SPEED_TIME, static_cast<long>(request.stallLimit.count()));

    // The given file replaces any default this libcurl was built with, for the
    // host and for a proxy reached over https.
    curl_easy_setopt(easy, CURLOPT_CAPATH, nullptr);
    curl_easy_setopt(easy, CURLOPT_PROXY_CAPATH, nullptr);
    if (request.certificateBundle.has_value()) {
        curl_easy_setopt(easy, CURLOPT_CAINFO, request.certificateBundle->c_str());
        curl_easy_setopt(easy, CURLOPT_PROXY_CAINFO, request.certificateBundle->c_str());
    }

    if (isThisMachine(target.host)) {
        // An empty proxy overrides the environment for this transfer alone.
        curl_easy_setopt(easy, CURLOPT_PROXY, "");
    }
}

FetchError transferError(const CURLcode code, const std::string& url, const char* errorText)
{
    std::string message{ errorText };
    if (message.empty()) {
        message = curl_easy_strerror(code);
    }
    if ((code == CURLE_PEER_FAILED_VERIFICATION) || (code == CURLE_SSL_CACERT_BADFILE)) {
        return CertificateFailure{ .url = url, .message = std::move(message) };
    }
    return TransferFailure{ .url = url, .message = std::move(message) };
}

/**
 * Where a response asks to go next, when it is a redirect.
 */
std::optional<std::string> redirectTarget(CURL* easy, const long status)
{
    if ((status < FirstRedirectStatus) || (status >= PastLastRedirectStatus)) {
        return std::nullopt;
    }
    const char* location = nullptr;
    curl_easy_getinfo(easy, CURLINFO_REDIRECT_URL, &location);
    if (location == nullptr) {
        return std::nullopt;
    }
    return std::string{ location };
}

/**
 * Follow @p request.url to its content, which goes to @p file.
 *
 * Redirects are followed here rather than by libcurl, which can limit their
 * schemes but not the hosts a scheme may lead to.
 */
/**
 * Where @p url leads, when it may be fetched after @p previous, the target of
 * the redirect to it, if any.
 */
std::expected<UrlTarget, FetchError> acceptedTarget(const std::string& url, const std::optional<UrlTarget>& previous, const FetchRequest& request)
{
    const std::optional<UrlTarget> target = readTarget(url);
    if ((! target.has_value()) || (! (previous.has_value() ? isAcceptedRedirect(*previous, *target) : isAccepted(*target)))) {
        return std::unexpected(UrlRefused{ .url = url });
    }
    if (needsCertificates(*target) && (! request.certificateBundle.has_value())) {
        return std::unexpected(NoCertificateBundle{ .url = url });
    }
    return *target;
}

/**
 * What went wrong in a transfer of @p url that libcurl ended with @p code, if
 * anything did.
 */
std::expected<void, FetchError> transferOutcome(const CURLcode code, const std::string& url, const FetchRequest& request, const BodySink& sink, const char* errorText)
{
    if (sink.tooLarge) {
        return std::unexpected(ContentTooLarge{ .url = url, .limit = request.sizeLimit.value_or(0) });
    }
    if (sink.failure) {
        return std::unexpected(CannotWriteDestination{ .path = request.destination, .code = sink.failure });
    }
    if (code != CURLE_OK) {
        return std::unexpected(transferError(code, url, errorText));
    }
    return {};
}

std::expected<void, FetchError> transfer(const FetchRequest& request, File::StagedFile& file)
{
    // These come before the handle, which holds on to them until it is cleaned up.
    std::array<char, CURL_ERROR_SIZE> errorText{};
    BodySink sink{ .easy = nullptr, .file = &file, .sizeLimit = request.sizeLimit, .received = 0, .tooLarge = false, .failure = {} };
    const EasyHandle easy{ curl_easy_init() };
    if (easy == nullptr) {
        return std::unexpected(TransferFailure{ .url = request.url, .message = "cannot set up a transfer" });
    }
    sink.easy = easy.get();

    auto headers = headerList(request);
    if (! headers.has_value()) {
        return std::unexpected(std::move(headers.error()));
    }

    std::string url = request.url;
    std::optional<UrlTarget> origin;
    std::optional<UrlTarget> previous;
    for (int hop = 0; hop <= RedirectLimit; ++hop) {
        const auto target = acceptedTarget(url, previous, request);
        if (! target.has_value()) {
            return std::unexpected(target.error());
        }
        if (! origin.has_value()) {
            origin = *target;
        }

        errorText.fill('\0');
        applyOptions(easy.get(), request, url, *target, sink, errorText.data());
        if (isSameOrigin(*origin, *target)) {
            curl_easy_setopt(easy.get(), CURLOPT_HTTPHEADER, headers->get());
        }
        const CURLcode code = curl_easy_perform(easy.get());
        if (auto outcome = transferOutcome(code, url, request, sink, errorText.data()); ! outcome.has_value()) {
            return outcome;
        }

        long status = 0;
        curl_easy_getinfo(easy.get(), CURLINFO_RESPONSE_CODE, &status);
        if (status == HttpOk) {
            return {};
        }
        auto next = redirectTarget(easy.get(), status);
        if (! next.has_value()) {
            std::string asked = (status == HttpUnauthorized) ? authenticationAsked(easy.get()) : std::string{};
            return std::unexpected(UnexpectedStatus{ .url = url, .status = status, .authenticate = std::move(asked) });
        }
        url = std::move(*next);
        previous = *target;
    }
    return std::unexpected(TooManyRedirects{ .url = url });
}

}  // anonymous namespace

std::expected<void, FetchError> fetchToFile(const FetchRequest& request)
{
    auto file = File::StagedFile::begin(request.destination);
    if (! file.has_value()) {
        return std::unexpected(CannotWriteDestination{ .path = request.destination, .code = file.error() });
    }
    if (auto done = transfer(request, *file); ! done.has_value()) {
        return std::unexpected(std::move(done.error()));
    }
    if (const std::error_code failure = file->commit()) {
        return std::unexpected(CannotWriteDestination{ .path = request.destination, .code = failure });
    }
    return {};
}

}  // namespace scrap::Fetch
