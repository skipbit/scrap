#include "ProjectModule.h"
#include "project/command/NewOperation.h"
#include "project/command/BuildOperation.h"
#include "project/command/RunOperation.h"
#include "project/command/CleanOperation.h"
#include "project/service/ProjectService.h"
#include <memory>

namespace scrap::project {

void ProjectModule::registerCommands(CommandDispatcher& dispatcher,
                                     std::shared_ptr<CLIParser> /* parser */,
                                     std::shared_ptr<Presenter> presenter) {
    // Create Mock service for now
    auto service = std::make_shared<service::MockProjectService>();

    // Create individual operations (with default template service)
    auto newOp = std::make_shared<command::NewOperation>(service);
    newOp->setPresenter(presenter);

    auto buildOp = std::make_shared<command::BuildOperation>(service);
    buildOp->setPresenter(presenter);

    auto runOp = std::make_shared<command::RunOperation>(service);
    runOp->setPresenter(presenter);

    auto cleanOp = std::make_shared<command::CleanOperation>(service);
    cleanOp->setPresenter(presenter);

    // Register operations
    dispatcher.registerOperation("new", newOp);
    dispatcher.registerOperation("build", buildOp);
    dispatcher.registerOperation("run", runOp);
    dispatcher.registerOperation("clean", cleanOp);
}

std::vector<std::pair<std::string, std::string>> ProjectModule::getAvailableCommands() {
    return {
        {"new", "Create a new C++ project"},
        {"build", "Compile the current project"},
        {"run", "Run the current project executable"},
        {"clean", "Remove build artifacts and cached files"}
    };
}

} // namespace scrap::project
