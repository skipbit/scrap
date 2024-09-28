#include "config.h"
#include "environment.h"

namespace scrap {

std::optional<config> config::default_config()
{
    const auto directory = environment::value("XDG_CONFIG_HOME")
        .or_else([]() {
            return environment::value("HOME").and_then([](const std::string& home) {
                return std::make_optional<std::string>(home + "/.config");
            });
        })
        .and_then([](const std::string& path) {
            return std::make_optional<std::string>(path + "/scrap");
        });

    return (directory ? std::make_optional<config>(config(directory.value())) : std::nullopt);
}

config::config(const std::string& directory)
    : _directory(directory)
{
}

}
