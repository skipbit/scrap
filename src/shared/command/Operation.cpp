#include "shared/command/Operation.h"
#include "shared/command/CommandOptions.h"
#include "shared/command/ParsedOptions.h"
#include "shared/presentation/Presenter.h"
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace scrap {

Operation::Operation() = default;

Operation::Operation(const Operation&) = default;

Operation& Operation::operator=(const Operation&) = default;

Operation::Operation(Operation&&) noexcept = default;

Operation& Operation::operator=(Operation&&) noexcept = default;

Operation::~Operation() = default;

void Operation::setPresenter(std::shared_ptr<Presenter> presenter)
{
    presenter_ = std::move(presenter);
}

std::shared_ptr<Presenter> Operation::presenter() const
{
    return presenter_;
}

void Operation::execute(const std::vector<std::string>& /*args*/)
{
    // Default implementation does nothing
}

CommandOptions Operation::describeOptions() const
{
    // Default implementation returns empty options
    return {};
}

void Operation::execute(const ParsedOptions& options)
{
    // Default implementation converts to legacy format for backward compatibility
    // Operations that override describeOptions() should also override this method
    const auto& args = options.positionalArgs();
    execute(args);
}

}  // namespace scrap
