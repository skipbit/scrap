#include "list.h"
#include "repository/repository.h"

#include <dross/platform/path.h>
#include <dross/platform/xdg.h>
#include <filesystem>
#include <iostream>
#include <system_error>

namespace scrap {

list::list()
{
}

list::~list()
{
}

void list::execute(const std::vector<command::option>&)
{
    const auto directory = dross::xdg("scrap").data_home();
    if (! directory.has_value()) {
        std::cerr << "error: data home not found" << std::endl;
        return;
    }

    if (! dross::path(directory.value()).exists()) {
        std::error_code err;
        if (! std::filesystem::create_directories(directory.value(), err)) {
            std::cerr << "error: can't create_directories = " << err.message() << std::endl;
            return;
        }
    }

    const auto path = dross::path(directory.value()).append("toolchain");
    if (! path.exists()) {
        repository(path).clone("https://github.com/skipbit/scrap-toolchain.git");
        std::cout << "clone toolchain" << std::endl;
    } else {
        repository(path).update();
        std::cout << "update toolchain" << std::endl;
    }

    std::cout << "list operation executed: " << directory.value() << std::endl;
}

}
