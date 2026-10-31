#pragma once

#include "toolchain/ToolchainVersion.h"

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace scrap::Toolchain {

/**
 * @brief Whether a toolchain is offered.
 */
enum class ToolchainStatus : std::uint8_t {
    Active,
    Deprecated,
    Disabled  ///< No longer offered, and not to be installed.
};

/**
 * @brief An ingot kept in an OCI registry, checked by its digest.
 */
struct RegistrySource {
    std::string registry;  ///< e.g. ghcr.io/skipbit/scrap-toolchain
    std::string tag;       ///< e.g. gcc:15.2.0
    std::string digest;    ///< sha256: and 64 lowercase hex digits.
};

/**
 * @brief An ingot fetched from a URL, checked by its SHA-256.
 */
struct UrlSource {
    std::string url;
    std::string sha256;  ///< 64 lowercase hex digits.
};

/**
 * @brief The archive of one toolchain for one OS and architecture.
 */
struct Ingot {
    std::string platform;  ///< e.g. linux, darwin
    std::string arch;      ///< e.g. x86_64, aarch64
    std::variant<RegistrySource, UrlSource> source;
    std::optional<ToolchainVersion> glibcVersion;  ///< The oldest glibc it runs on, when it needs one.
};

/**
 * @brief One version of one toolchain family, with its ingots.
 */
struct IndexedToolchain {
    std::string family;
    ToolchainVersion version;
    ToolchainStatus status = ToolchainStatus::Active;
    std::string compiler;  ///< The program to run, e.g. g++ or clang.
    std::optional<ToolchainVersion> minScrapVersion;
    std::vector<Ingot> ingots;  ///< Empty when none has been built, or all were withdrawn.
};

/**
 * @brief The toolchains a channel offers.
 */
struct ToolchainIndex {
    std::vector<IndexedToolchain> toolchains;
};

}  // namespace scrap::Toolchain
