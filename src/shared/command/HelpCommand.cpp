#include "shared/command/HelpCommand.h"
#include "shared/presentation/Presenter.h"

namespace scrap {

HelpCommand::HelpCommand(std::shared_ptr<CLIParser> parser, const std::string& commandPath)
    : parser_(parser), commandPath_(commandPath)
{
}

HelpCommand::~HelpCommand() = default;

void HelpCommand::execute(const std::vector<std::string>& /*args*/)
{
    auto presenter = getPresenter();
    if (!presenter) {
        return; // No presenter available
    }

    if (parser_) {
        std::string helpText = parser_->getHelpText(commandPath_);
        presenter->showHelp(helpText);
    } else {
        presenter->showError("Help system not available");
    }
}

} // namespace scrap
