#pragma once

#include "command.h"

namespace scrap {

class toolchain : public command {
public:
    toolchain();
    ~toolchain() override;
};

}
