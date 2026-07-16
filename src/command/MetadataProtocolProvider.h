#pragma once

#include "command/ExternalMetadataProvider.h"

#include <chrono>
#include <expected>
#include <filesystem>
#include <string>

namespace scrap::Command {

/**
 * @brief Fetches command metadata from an external executable via the
 * `--scrap-metadata` protocol, falling back to `--help`.
 *
 * The executable is invoked as a subprocess (no shell, no PATH search — the
 * path is canonicalized and passed directly to posix_spawn). Only the first
 * non-empty line of stdout is used as a plain-text description; structured
 * (name/options) metadata is not yet part of the protocol.
 */
class MetadataProtocolProvider final : public ExternalMetadataProvider {
public:
    /**
     * @brief Construct with an optional subprocess timeout.
     *
     * @param timeout Maximum time to wait for a probe subprocess before it
     *   is killed and treated as a failed attempt. Exposed so tests can pass
     *   a short value and keep runtime bounded.
     */
    explicit MetadataProtocolProvider(std::chrono::milliseconds timeout = DefaultTimeout);

    /**
     * @brief Fetch metadata by running `<executable> --scrap-metadata`,
     * falling back to `<executable> --help` if that attempt fails.
     *
     * @param executable Path to the scrap-* executable.
     * @return Metadata with a plain-text description on success, or an
     *   error message describing why metadata could not be fetched.
     */
    [[nodiscard]] auto
    fetch(const std::filesystem::path& executable) -> std::expected<ExternalCommandMetadata, std::string> override;

private:
    static constexpr std::chrono::milliseconds DefaultTimeout{2000};

    std::chrono::milliseconds timeout_;
};

}  // namespace scrap::Command
