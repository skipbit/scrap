#include "toolchain.h"
#include "list.h"

namespace scrap {

toolchain::toolchain()
{
    add("list", list());
}

toolchain::~toolchain()
{
}

}
