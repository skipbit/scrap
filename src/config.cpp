#include "config.h"

#include <dross/platform/xdg.h>

namespace scrap {

std::optional<config> config::default_config()
{
    const auto directory = dross::xdg("scrap").config_home();
    return (directory ? std::make_optional<config>(config(directory.value())) : std::nullopt);
}

config::config(const std::string& directory)
    : _directory(directory)
{
}

}
