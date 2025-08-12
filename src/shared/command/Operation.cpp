#include "shared/command/Operation.h"
#include "shared/presentation/Presenter.h"

namespace scrap {

Operation::Operation() = default;

Operation::Operation(const Operation&) = default;

Operation::~Operation() = default;

void Operation::setPresenter(std::shared_ptr<Presenter> presenter)
{
    presenter_ = presenter;
}

std::shared_ptr<Presenter> Operation::presenter() const
{
    return presenter_;
}

void Operation::execute(const std::vector<std::string>& /*args*/)
{
    // Default implementation does nothing
}

} // namespace scrap
