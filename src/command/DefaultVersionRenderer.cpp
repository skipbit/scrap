#include "command/DefaultVersionRenderer.h"

#include "shared/constants/version.h"

#include <string>

namespace scrap::Command {

/**
 * Return the application version string.
 */
std::string DefaultVersionRenderer::render() const
{
    return scrap::version();
}

}  // namespace scrap::Command
