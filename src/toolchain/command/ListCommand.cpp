#include "toolchain/command/ListCommand.h"
#include "repository/model/Repository.h"

#include <dross/platform/path.h>
#include <dross/platform/xdg.h>
#include <filesystem>
#include <iostream>
#include <system_error>

namespace scrap::toolchain {

ListCommand::ListCommand()
{
}

ListCommand::~ListCommand()
{
}

void ListCommand::execute(const std::vector<Command::Option>&)
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
        Repository(path).clone("https://github.com/skipbit/scrap-toolchain.git");
        std::cout << "clone toolchain" << std::endl;
    } else {
        Repository(path).update();
        std::cout << "update toolchain" << std::endl;
    }

    std::cout << "list operation executed: " << directory.value() << std::endl;
}

}
