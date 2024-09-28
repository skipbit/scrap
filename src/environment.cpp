#include "environment.h"

#include <stdlib.h>

namespace scrap {

std::optional<std::string> environment::value(const std::string& key)
{
    const char* value = getenv(key.c_str());
    return (value ? std::make_optional<std::string>(value) : std::nullopt);
}

}
