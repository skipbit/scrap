#pragma once

#include <string>

namespace scrap {

// Version banner, e.g. "scrap 0.1.0" on a release or
// "scrap 0.1.0 (v0.1.0-2-g2a761c0)" on a build between releases.
std::string version();

}  // namespace scrap
