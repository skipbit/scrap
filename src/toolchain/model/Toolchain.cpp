#include "toolchain/model/Toolchain.h"

namespace scrap::toolchain {

Toolchain::Toolchain(const std::string& name, const std::string& version, const std::string& architecture)
    : name_(name), version_(version), architecture_(architecture), isDefault_(false)
{
}

const std::string& Toolchain::getName() const
{
    return name_;
}

const std::string& Toolchain::getVersion() const
{
    return version_;
}

const std::string& Toolchain::getArchitecture() const
{
    return architecture_;
}

std::string Toolchain::getFullIdentifier() const
{
    return name_ + "-" + version_ + "-" + architecture_;
}

bool Toolchain::isDefault() const
{
    return isDefault_;
}

void Toolchain::setDefault(bool isDefault)
{
    isDefault_ = isDefault;
}

}