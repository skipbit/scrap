#pragma once

#include <string>
#include <filesystem>
#include <optional>

namespace scrap::toolchain::model {

/**
 * @brief Value object for toolchain identifier
 */
class ToolchainId {
public:
    explicit ToolchainId(const std::string& value) : value_(value) {}
    const std::string& value() const { return value_; }
    bool operator==(const ToolchainId& other) const { return value_ == other.value_; }
    bool operator<(const ToolchainId& other) const { return value_ < other.value_; }

private:
    std::string value_;
};

/**
 * @brief Value object for toolchain name
 */
class ToolchainName {
public:
    explicit ToolchainName(const std::string& value) : value_(value) {}
    const std::string& value() const { return value_; }
    std::string toString() const { return value_; }

private:
    std::string value_;
};

/**
 * @brief Value object for version
 */
class Version {
public:
    explicit Version(const std::string& value) : value_(value) {}
    const std::string& value() const { return value_; }
    std::string toString() const { return value_; }

private:
    std::string value_;
};

/**
 * @brief Enumeration for CPU architecture
 */
enum class Architecture {
    X86_64,
    ARM64,
    Unknown
};

/**
 * @brief Enumeration for platform/OS
 */
enum class Platform {
    Linux,
    Darwin,  // macOS
    Windows,
    Unknown
};

/**
 * @brief Toolchain specification for installation
 */
struct ToolchainSpecification {
    std::string name;
    std::string version;
    std::optional<Architecture> architecture;
    std::optional<Platform> platform;

    static ToolchainSpecification parse(const std::string& spec);
};

/**
 * @brief Domain model representing a toolchain
 *
 * This class encapsulates the concept of a toolchain in the scrap ecosystem,
 * following Domain-Driven Design principles.
 */
class Toolchain {
public:
    Toolchain(const ToolchainId& id,
              const ToolchainName& name,
              const Version& version,
              Architecture architecture,
              Platform platform);

    // Getters
    const ToolchainId& id() const { return id_; }
    const ToolchainName& name() const { return name_; }
    const Version& version() const { return version_; }
    Architecture architecture() const { return architecture_; }
    Platform platform() const { return platform_; }
    const std::optional<std::filesystem::path>& installationPath() const { return path_; }
    bool isSelected() const { return isSelected_; }

    // Setters for mutable properties
    void setInstallationPath(const std::filesystem::path& path) { path_ = path; }
    void setSelected(bool selected) { isSelected_ = selected; }

    // Business logic
    std::string fullName() const;
    std::string triple() const;  // e.g., "llvm-18.0.0-x86_64-darwin"
    bool isInstalled() const { return path_.has_value(); }

private:
    ToolchainId id_;
    ToolchainName name_;
    Version version_;
    Architecture architecture_;
    Platform platform_;
    std::optional<std::filesystem::path> path_;
    bool isSelected_ = false;
};

/**
 * @brief Policy interface for toolchain operations
 */
class ToolchainPolicy {
public:
    virtual ~ToolchainPolicy() = default;
    virtual bool canInstall(const Toolchain& toolchain) const = 0;
    virtual bool canSelect(const Toolchain& toolchain) const = 0;
    virtual bool canRemove(const Toolchain& toolchain) const = 0;
};

/**
 * @brief Default implementation of toolchain policy
 */
class DefaultToolchainPolicy : public ToolchainPolicy {
public:
    bool canInstall(const Toolchain& toolchain) const override;
    bool canSelect(const Toolchain& toolchain) const override;
    bool canRemove(const Toolchain& toolchain) const override;
};

// Helper functions
std::string architectureToString(const Architecture arch);
std::string platformToString(const Platform platform);
Architecture stringToArchitecture(const std::string& str);
Platform stringToPlatform(const std::string& str);
Architecture getCurrentArchitecture();
Platform getCurrentPlatform();

} // namespace scrap::toolchain::model
