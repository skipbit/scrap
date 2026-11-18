#pragma once

#include "command/RuntimeEnvironment.h"
#include "toolchain/IngotSelection.h"
#include "toolchain/ToolchainIndex.h"
#include "toolchain/ToolchainVersion.h"

#include <filesystem>
#include <optional>
#include <string>

namespace scrap::Command {

/**
 * @brief The saved copy of the index, read to warn of installed toolchains
 *        it no longer offers as it did. Nothing is fetched, and a copy that
 *        is missing or cannot be read warns of nothing.
 */
class SavedStatusWarning {
public:
    /**
     * @brief Read the copy saved below @p scrapHome for the index @p env
     *        names, as it stands for this host.
     */
    [[nodiscard]] static SavedStatusWarning read(const RuntimeEnvironment& env, const std::filesystem::path& scrapHome);

    /**
     * @brief The warning lines for @p family at @p version; empty when there
     *        is nothing to warn of, or the copy cannot tell.
     */
    [[nodiscard]] std::string linesFor(const std::string& family, const Toolchain::ToolchainVersion& version) const;

private:
    SavedStatusWarning(std::optional<Toolchain::ToolchainIndex> index, std::optional<Toolchain::InstallingHost> host);

    std::optional<Toolchain::ToolchainIndex> _index;
    std::optional<Toolchain::InstallingHost> _host;
};

}  // namespace scrap::Command
