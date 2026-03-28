#include "command/DefaultVersionRenderer.h"

#include "shared/constants/version.h"

namespace scrap::Command {

auto DefaultVersionRenderer::render() const -> std::string
{
    return scrap::version();
}

}  // namespace scrap::Command
