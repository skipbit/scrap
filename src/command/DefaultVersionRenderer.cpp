#include "command/DefaultVersionRenderer.h"

#include "shared/constants/version.h"

#include <string>

namespace scrap::Command {

/**
 * Return the application version string.
 */
auto DefaultVersionRenderer::render() const -> std::string
{
    return scrap::version();
}

}  // namespace scrap::Command
