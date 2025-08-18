#include "ToolchainReference.h"
#include <dross/type/error.h>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

// Error code creation function (global scope for ADL)
std::error_code make_error_code(scrap::Configuration::Model::ToolchainReferenceError e) noexcept
{
    struct ToolchainReferenceErrorCategory : std::error_category {
        [[nodiscard]] const char* name() const noexcept override
        {
            return "ToolchainReference";
        }

        [[nodiscard]] std::string message(int ev) const override
        {
            switch (static_cast<scrap::Configuration::Model::ToolchainReferenceError>(ev)) {
                case scrap::Configuration::Model::ToolchainReferenceError::EmptyName:
                    return "Toolchain name cannot be empty";
                case scrap::Configuration::Model::ToolchainReferenceError::EmptySpecification:
                    return "Toolchain specification cannot be empty";
                case scrap::Configuration::Model::ToolchainReferenceError::EmptyVersion:
                    return "Version cannot be empty after '@'";
                case scrap::Configuration::Model::ToolchainReferenceError::SystemDefaultHasNoName:
                    return "System default toolchain has no name";
                default:
                    return "Unknown ToolchainReference error";
            }
        }
    };

    static const ToolchainReferenceErrorCategory ErrorCategory{};
    return {static_cast<int>(e), ErrorCategory};
}

namespace scrap::Configuration::Model {

// PIMPL Implementation
class ToolchainReference::Internal {
public:
    bool isSystemDefault = false;
    std::string name;
    std::optional<std::string> version;

    // Default constructor for system default
    Internal()
        : isSystemDefault(true)
    {
    }

    // Constructor for managed toolchain
    Internal(std::string toolchainName, std::optional<std::string> toolchainVersion)
        : name(std::move(toolchainName)), version(std::move(toolchainVersion))
    {
    }
};

ToolchainReference::ToolchainReference()
    : impl_(std::make_unique<Internal>())
{
}

ToolchainReference::ToolchainReference(const ToolchainReference& other)
    : impl_(std::make_unique<Internal>(*other.impl_))
{
}

ToolchainReference& ToolchainReference::operator=(const ToolchainReference& other)
{
    if (this != &other) {
        *impl_ = *other.impl_;
    }
    return *this;
}

ToolchainReference::ToolchainReference(ToolchainReference&& other) noexcept = default;

ToolchainReference& ToolchainReference::operator=(ToolchainReference&& other) noexcept = default;

ToolchainReference::~ToolchainReference() = default;

ToolchainReference ToolchainReference::createSystemDefault()
{
    ToolchainReference result;
    result.impl_ = std::make_unique<Internal>();
    return result;
}

std::expected<ToolchainReference, dross::error>
ToolchainReference::createManaged(const std::string& name, const std::optional<std::string>& version) noexcept
{
    if (name.empty()) {
        auto errorCode = make_error_code(ToolchainReferenceError::EmptyName);
        return std::unexpected(dross::error{errorCode.value(), errorCode.category()});
    }
    ToolchainReference result;
    result.impl_ = std::make_unique<Internal>(name, version);
    return result;
}

std::expected<ToolchainReference, dross::error> ToolchainReference::parse(const std::string& spec) noexcept
{
    if (spec.empty()) {
        auto errorCode = make_error_code(ToolchainReferenceError::EmptySpecification);
        return std::unexpected(dross::error{errorCode.value(), errorCode.category()});
    }

    if (spec == "system" || spec == "system-default") {
        return createSystemDefault();
    }

    auto atPos = spec.find('@');
    if (atPos == std::string::npos) {
        // No version specified
        auto result = createManaged(spec);
        if (!result) {
            return std::unexpected(result.error());
        }
        return result.value();
    }

    const std::string name = spec.substr(0, atPos);
    std::string version = spec.substr(atPos + 1);

    if (version.empty()) {
        auto errorCode = make_error_code(ToolchainReferenceError::EmptyVersion);
        return std::unexpected(dross::error{errorCode.value(), errorCode.category()});
    }

    auto result = createManaged(name, version);
    if (!result) {
        return std::unexpected(result.error());
    }
    return result.value();
}

bool ToolchainReference::operator==(const ToolchainReference& other) const
{
    return impl_->isSystemDefault == other.impl_->isSystemDefault && impl_->name == other.impl_->name &&
        impl_->version == other.impl_->version;
}

bool ToolchainReference::operator!=(const ToolchainReference& other) const
{
    return !(*this == other);
}

bool ToolchainReference::isSystemDefault() const noexcept
{
    return impl_->isSystemDefault;
}

std::expected<std::string, dross::error> ToolchainReference::name() const noexcept
{
    if (impl_->isSystemDefault) {
        auto errorCode = make_error_code(ToolchainReferenceError::SystemDefaultHasNoName);
        return std::unexpected(dross::error{errorCode.value(), errorCode.category()});
    }
    return impl_->name;
}

const std::optional<std::string>& ToolchainReference::version() const noexcept
{
    return impl_->version;
}

std::string ToolchainReference::toString() const
{
    if (impl_->isSystemDefault) {
        return "system";
    }

    std::string result = impl_->name;
    if (impl_->version) {
        result += "@" + *impl_->version;
    }
    return result;
}

}  // namespace scrap::Configuration::Model
