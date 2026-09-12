#include "project/ManifestError.h"

#include <string>

namespace scrap::Project {

/**
 * Render an error as "<file>[:<line>:<column>]: [<key>: ]<message>".
 */
auto describe(const ManifestError& error) -> std::string
{
    std::string line = error.file.string();
    if (error.position.has_value()) {
        line += ':';
        line += std::to_string(error.position->line);
        line += ':';
        line += std::to_string(error.position->column);
    }
    line += ": ";
    if (! error.key.empty()) {
        line += error.key;
        line += ": ";
    }
    line += error.message;
    return line;
}

}  // namespace scrap::Project
