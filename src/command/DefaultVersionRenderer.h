#pragma once

#include "command/VersionRenderer.h"

namespace scrap::Command {

/**
 * Default implementation of VersionRenderer.
 *
 * Returns the application version string from scrap::version().
 */
class DefaultVersionRenderer : public VersionRenderer {
public:
    [[nodiscard]] auto render() const -> std::string override;
};

}  // namespace scrap::Command
