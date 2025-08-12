#pragma once

#include <string>
#include <optional>
#include <stdexcept>

namespace scrap::configuration::model {

/**
 * @brief Value Object representing a toolchain reference
 *
 * Can represent either:
 * - A managed toolchain (name + optional version)
 * - System default toolchain (not managed by scrap)
 */
class ToolchainReference {
public:
    /**
     * @brief Default constructor creates system default toolchain reference
     */
    ToolchainReference() : isSystemDefault_(true) {}

    /**
     * @brief Create system default toolchain reference
     */
    static ToolchainReference createSystemDefault() {
        return ToolchainReference();
    }

    /**
     * @brief Create managed toolchain reference
     * @param name Toolchain name (e.g., "llvm", "gcc")
     * @param version Optional version (e.g., "18.0.0")
     */
    static ToolchainReference createManaged(const std::string& name,
                                          const std::optional<std::string>& version = std::nullopt) {
        if (name.empty()) {
            throw std::invalid_argument("Toolchain name cannot be empty");
        }
        return ToolchainReference(name, version);
    }

    /**
     * @brief Parse toolchain reference from string
     * @param spec Specification string (e.g., "llvm@18.0.0", "gcc", "system")
     */
    static ToolchainReference parse(const std::string& spec) {
        if (spec.empty()) {
            throw std::invalid_argument("Toolchain specification cannot be empty");
        }

        if (spec == "system" || spec == "system-default") {
            return createSystemDefault();
        }

        auto atPos = spec.find('@');
        if (atPos == std::string::npos) {
            // No version specified
            return createManaged(spec);
        }

        std::string name = spec.substr(0, atPos);
        std::string version = spec.substr(atPos + 1);

        if (version.empty()) {
            throw std::invalid_argument("Version cannot be empty after '@'");
        }

        return createManaged(name, version);
    }

    // Value Object - equality comparison
    bool operator==(const ToolchainReference& other) const {
        return isSystemDefault_ == other.isSystemDefault_ &&
               name_ == other.name_ &&
               version_ == other.version_;
    }

    bool operator!=(const ToolchainReference& other) const {
        return !(*this == other);
    }

    /**
     * @brief Check if this represents system default toolchain
     */
    bool isSystemDefault() const {
        return isSystemDefault_;
    }

    /**
     * @brief Get toolchain name (empty for system default)
     */
    const std::string& name() const {
        if (isSystemDefault_) {
            throw std::logic_error("System default toolchain has no name");
        }
        return name_;
    }

    /**
     * @brief Get toolchain version (nullopt if no version specified or system default)
     */
    const std::optional<std::string>& version() const {
        return version_;
    }

    /**
     * @brief Convert to string representation
     */
    std::string toString() const {
        if (isSystemDefault_) {
            return "system";
        }

        std::string result = name_;
        if (version_) {
            result += "@" + *version_;
        }
        return result;
    }

private:
    // Private constructor for managed toolchain
    ToolchainReference(const std::string& name, const std::optional<std::string>& version)
        : isSystemDefault_(false), name_(name), version_(version) {}

    bool isSystemDefault_ = false;
    std::string name_;
    std::optional<std::string> version_;
};

} // namespace scrap::configuration::model
