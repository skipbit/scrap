#include "support/FakeOciRegistry.h"

#include "support/HttpTestServer.h"

#include <string>
#include <string_view>
#include <utility>

namespace scrap::TestSupport {

namespace {

HttpTestResponse ok(std::string body)
{
    return { .status = 200, .headers = {}, .body = std::move(body), .stalls = false };
}

std::string manifestFor(const std::string& layerDigest, const std::string& ingot)
{
    return R"({"schemaVersion": 2, "mediaType": "application/vnd.oci.image.manifest.v1+json", "layers": [{"mediaType": "application/x-tar+xz", "digest": ")" + layerDigest
           + R"(", "size": )" + std::to_string(ingot.size()) + "}]}";
}

}  // anonymous namespace

FakeOciRegistry::FakeOciRegistry(HttpTestServer& server, std::string repository, const std::string& ingot, HexDigest sha256Hex, FakeOciRegistryOptions options)
    : _server(server)
    , _repository(std::move(repository))
    , _sha256Hex(std::move(sha256Hex))
    , _options(std::move(options))
    , _layerDigest(digestOf(ingot))
{
    serveManifest(manifestFor(_layerDigest, ingot));
    const std::string blob = _options.blob.empty() ? ingot : _options.blob;
    if (! _options.token.empty()) {
        _server.respond(tokenPath(_repository, _options.service), ok(R"({"token": ")" + _options.token + R"("})"));
    }
    if (_options.storagePath.empty()) {
        _server.respond(blobPath(), locked(ok(blob)));
    } else {
        _server.respond(blobPath(), locked({ .status = 307, .headers = "Location: http://localhost:" + std::to_string(_server.port()) + _options.storagePath + "\r\n", .body = {}, .stalls = false }));
        _server.respond(_options.storagePath, ok(blob));
    }
}

void FakeOciRegistry::serveManifest(const std::string& manifest)
{
    _manifestDigest = digestOf(manifest);
    _server.respond(manifestPath(), locked(ok(manifest)));
}

const std::string& FakeOciRegistry::manifestDigest() const
{
    return _manifestDigest;
}

std::string FakeOciRegistry::manifestPath() const
{
    return "/v2/" + _repository + "/manifests/" + _manifestDigest;
}

std::string FakeOciRegistry::blobPath() const
{
    return "/v2/" + _repository + "/blobs/" + _layerDigest;
}

std::string FakeOciRegistry::tokenPath(const std::string& repository, const std::string& service)
{
    std::string scope;
    for (const char c : repository) {
        scope += (c == '/') ? std::string{ "%2F" } : std::string{ c };
    }
    return "/token?service=" + service + "&scope=repository%3A" + scope + "%3Apull";
}

HttpTestResponse FakeOciRegistry::locked(HttpTestResponse response) const
{
    if (! _options.token.empty()) {
        response.requiredHeader = "Authorization: Bearer " + _options.token;
        response.challenge = challenge();
    }
    return response;
}

std::string FakeOciRegistry::digestOf(const std::string_view bytes) const
{
    return "sha256:" + _sha256Hex(bytes);
}

std::string FakeOciRegistry::challenge() const
{
    return R"(Bearer realm="http://127.0.0.1:)" + std::to_string(_server.port()) + R"(/token",service=")" + _options.service + R"(")";
}

}  // namespace scrap::TestSupport
