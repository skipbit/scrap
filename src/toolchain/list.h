#pragma once

#include "operation.h"

namespace scrap {

class list : public scrap::operation {
public:
    list();
    ~list() override;

    void execute(const std::vector<command::option>&) override;
};

}
