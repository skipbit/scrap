#include "CommandOptions.h"

namespace scrap {

// CommandOption::Impl
class CommandOption::Impl {
public:
    std::string name_;
    std::string description_;
    OptionType type_;
    std::optional<std::string> defaultValue_;
    bool required_ = false;
    std::vector<std::string> choices_;
    std::string shortName_;

    Impl(const std::string& name, const std::string& description, OptionType type)
        : name_(name), description_(description), type_(type)
    {
    }
};

// CommandOption implementation
CommandOption::CommandOption(const std::string& name, const std::string& description,
                           OptionType type)
    : impl_(std::make_unique<Impl>(name, description, type))
{
}

CommandOption::~CommandOption() = default;

CommandOption::CommandOption(const CommandOption& other)
    : impl_(std::make_unique<Impl>(*other.impl_))
{
}

CommandOption& CommandOption::operator=(const CommandOption& other)
{
    if (this != &other) {
        impl_ = std::make_unique<Impl>(*other.impl_);
    }
    return *this;
}

CommandOption::CommandOption(CommandOption&& other) noexcept = default;

CommandOption& CommandOption::operator=(CommandOption&& other) noexcept = default;

const std::string& CommandOption::name() const
{
    return impl_->name_;
}

const std::string& CommandOption::description() const
{
    return impl_->description_;
}

OptionType CommandOption::type() const
{
    return impl_->type_;
}

const std::optional<std::string>& CommandOption::defaultValue() const
{
    return impl_->defaultValue_;
}

bool CommandOption::required() const
{
    return impl_->required_;
}

const std::vector<std::string>& CommandOption::choices() const
{
    return impl_->choices_;
}

const std::string& CommandOption::shortName() const
{
    return impl_->shortName_;
}

CommandOption& CommandOption::withDefault(const std::string& value)
{
    impl_->defaultValue_ = value;
    return *this;
}

CommandOption& CommandOption::withRequired(bool required)
{
    impl_->required_ = required;
    return *this;
}

CommandOption& CommandOption::withChoices(const std::vector<std::string>& choices)
{
    impl_->choices_ = choices;
    return *this;
}

CommandOption& CommandOption::withShortName(const std::string& shortName)
{
    impl_->shortName_ = shortName;
    return *this;
}

// CommandOptions::Impl
class CommandOptions::Impl {
public:
    std::vector<CommandOption> positionals_;
    std::vector<CommandOption> options_;
    std::vector<CommandOption> flags_;
};

// CommandOptions implementation
CommandOptions::CommandOptions()
    : impl_(std::make_unique<Impl>())
{
}

CommandOptions::~CommandOptions() = default;

CommandOptions::CommandOptions(const CommandOptions& other)
    : impl_(std::make_unique<Impl>(*other.impl_))
{
}

CommandOptions& CommandOptions::operator=(const CommandOptions& other)
{
    if (this != &other) {
        impl_ = std::make_unique<Impl>(*other.impl_);
    }
    return *this;
}

CommandOptions::CommandOptions(CommandOptions&& other) noexcept = default;

CommandOptions& CommandOptions::operator=(CommandOptions&& other) noexcept = default;

CommandOptions& CommandOptions::addPositional(const std::string& name, const std::string& description)
{
    impl_->positionals_.emplace_back(name, description, OptionType::String);
    return *this;
}

CommandOptions& CommandOptions::addOption(const CommandOption& option)
{
    impl_->options_.push_back(option);
    return *this;
}

CommandOptions& CommandOptions::addFlag(const std::string& name, const std::string& description)
{
    impl_->flags_.emplace_back(name, description, OptionType::Flag);
    return *this;
}

CommandOptions& CommandOptions::addFlag(const std::string& name, const std::string& shortName,
                                       const std::string& description)
{
    impl_->flags_.emplace_back(name, description, OptionType::Flag).withShortName(shortName);
    return *this;
}

const std::vector<CommandOption>& CommandOptions::positionals() const
{
    return impl_->positionals_;
}

const std::vector<CommandOption>& CommandOptions::options() const
{
    return impl_->options_;
}

const std::vector<CommandOption>& CommandOptions::flags() const
{
    return impl_->flags_;
}

bool CommandOptions::hasOptions() const
{
    return !impl_->positionals_.empty() || !impl_->options_.empty() || !impl_->flags_.empty();
}

}
