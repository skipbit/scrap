#include "ParsedOptions.h"
#include <unordered_map>

namespace scrap {

// ParsedOptions::Impl
class ParsedOptions::Impl {
public:
    std::unordered_map<std::string, Value> values_;
    std::vector<std::string> positionalArgs_;
};

// ParsedOptions implementation
ParsedOptions::ParsedOptions()
    : impl_(std::make_unique<Impl>())
{
}

ParsedOptions::~ParsedOptions() = default;

ParsedOptions::ParsedOptions(const ParsedOptions& other)
    : impl_(std::make_unique<Impl>(*other.impl_))
{
}

ParsedOptions& ParsedOptions::operator=(const ParsedOptions& other)
{
    if (this != &other) {
        impl_ = std::make_unique<Impl>(*other.impl_);
    }
    return *this;
}

ParsedOptions::ParsedOptions(ParsedOptions&& other) noexcept = default;

ParsedOptions& ParsedOptions::operator=(ParsedOptions&& other) noexcept = default;

void ParsedOptions::set(const std::string& key, const Value& value)
{
    impl_->values_[key] = value;
}

std::optional<std::string> ParsedOptions::string(const std::string& key) const
{
    auto it = impl_->values_.find(key);
    if (it != impl_->values_.end()) {
        if (const auto* str = std::get_if<std::string>(&it->second)) {
            return *str;
        }
    }
    return std::nullopt;
}

std::optional<int> ParsedOptions::integer(const std::string& key) const
{
    auto it = impl_->values_.find(key);
    if (it != impl_->values_.end()) {
        if (const auto* intVal = std::get_if<int>(&it->second)) {
            return *intVal;
        }
    }
    return std::nullopt;
}

bool ParsedOptions::flag(const std::string& key) const
{
    auto it = impl_->values_.find(key);
    if (it != impl_->values_.end()) {
        if (const auto* boolVal = std::get_if<bool>(&it->second)) {
            return *boolVal;
        }
    }
    return false;
}

std::vector<std::string> ParsedOptions::stringList(const std::string& key) const
{
    auto it = impl_->values_.find(key);
    if (it != impl_->values_.end()) {
        if (const auto* vec = std::get_if<std::vector<std::string>>(&it->second)) {
            return *vec;
        }
    }
    return {};
}

bool ParsedOptions::has(const std::string& key) const
{
    return impl_->values_.find(key) != impl_->values_.end();
}

const std::vector<std::string>& ParsedOptions::positionalArgs() const
{
    return impl_->positionalArgs_;
}

void ParsedOptions::setPositionalArgs(const std::vector<std::string>& args)
{
    impl_->positionalArgs_ = args;
}

}
