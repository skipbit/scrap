#pragma once

#include "command/VersionRenderer.h"

namespace scrap::Command {

/**
 * @brief Default implementation of VersionRenderer.
 *
 * Returns the application version string obtained from scrap::version().
 */
class DefaultVersionRenderer : public VersionRenderer {
public:
    /**
     * @brief Render the version string.
     *
     * @return Version string (e.g. "scrap 0.0.1").
     */
    [[nodiscard]] auto render() const -> std::string override;
};

}  // namespace scrap::Command
