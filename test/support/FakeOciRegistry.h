#pragma once

#include "support/HttpTestServer.h"

#include <functional>
#include <string>
#include <string_view>

namespace scrap::TestSupport {

/**
 * @brief How a FakeOciRegistry serves.
 */
struct FakeOciRegistryOptions {
    /// Asks for this bearer token, as ghcr.io does for an anonymous pull, and
    /// hands it out at /token. Asks for none when empty.
    std::string token;

    /// The service the challenge for the token names.
    std::string service;

    /// A path on another host name for this machine that the blob redirects to,
    /// which must not see the token. The blob is served in place when empty.
    std::string storagePath;

    /// Served as the blob in place of the ingot, when not empty.
    std::string blob;
};

/**
 * @brief The answers of an OCI registry that keeps one ingot as the single layer
 *        of a manifest, put on an HttpTestServer that outlives it.
 */
class FakeOciRegistry {
public:
    /// SHA-256 of bytes in lowercase hexadecimal.
    using HexDigest = std::function<std::string(std::string_view)>;

    FakeOciRegistry(HttpTestServer& server, std::string repository, const std::string& ingot, HexDigest sha256Hex, FakeOciRegistryOptions options);

    /// Serve @p manifest from now on, under its own digest.
    void serveManifest(const std::string& manifest);

    [[nodiscard]] const std::string& manifestDigest() const;
    [[nodiscard]] std::string manifestPath() const;
    [[nodiscard]] std::string blobPath() const;

    /// Where the token endpoint is asked for a pull of @p repository.
    [[nodiscard]] static std::string tokenPath(const std::string& repository, const std::string& service);

    /// @p response, behind the token when the registry asks for one.
    [[nodiscard]] HttpTestResponse locked(HttpTestResponse response) const;

private:
    [[nodiscard]] std::string digestOf(std::string_view bytes) const;
    [[nodiscard]] std::string challenge() const;

    HttpTestServer& _server;
    std::string _repository;
    HexDigest _sha256Hex;
    FakeOciRegistryOptions _options;
    std::string _layerDigest;
    std::string _manifestDigest;
};

}  // namespace scrap::TestSupport
