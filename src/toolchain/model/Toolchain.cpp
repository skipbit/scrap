#include "Toolchain.h"
#include <sstream>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#else
#include <sys/utsname.h>
#endif

namespace scrap::toolchain::model {

// ToolchainId implementation
ToolchainId::ToolchainId(const std::string& value) : value_(value)
{
}

const std::string& ToolchainId::value() const
{
    return value_;
}

bool ToolchainId::operator==(const ToolchainId& other) const
{
    return value_ == other.value_;
}

bool ToolchainId::operator<(const ToolchainId& other) const
{
    return value_ < other.value_;
}

// ToolchainName implementation
ToolchainName::ToolchainName(const std::string& value) : value_(value)
{
}

const std::string& ToolchainName::value() const
{
    return value_;
}

std::string ToolchainName::toString() const
{
    return value_;
}

// Version implementation
Version::Version(const std::string& value) : value_(value)
{
}

const std::string& Version::value() const
{
    return value_;
}

std::string Version::toString() const
{
    return value_;
}

// Toolchain implementation
Toolchain::Toolchain(const ToolchainId& id,
                     const ToolchainName& name,
                     const Version& version,
                     Architecture architecture,
                     Platform platform)
    : id_(id)
    , name_(name)
    , version_(version)
    , architecture_(architecture)
    , platform_(platform)
    , isSelected_(false)
{
}

const ToolchainId& Toolchain::id() const
{
    return id_;
}

const ToolchainName& Toolchain::name() const
{
    return name_;
}

const Version& Toolchain::version() const
{
    return version_;
}

Architecture Toolchain::architecture() const
{
    return architecture_;
}

Platform Toolchain::platform() const
{
    return platform_;
}

const std::optional<std::filesystem::path>& Toolchain::installationPath() const
{
    return path_;
}

bool Toolchain::isSelected() const
{
    return isSelected_;
}

void Toolchain::setInstallationPath(const std::filesystem::path& path)
{
    path_ = path;
}

void Toolchain::setSelected(bool selected)
{
    isSelected_ = selected;
}

bool Toolchain::isInstalled() const
{
    return path_.has_value();
}

std::string Toolchain::fullName() const
{
    std::stringstream ss;
    ss << name_.toString() << " " << version_.toString();
    return ss.str();
}

std::string Toolchain::triple() const
{
    std::stringstream ss;
    ss << name_.toString() << "-" << version_.toString()
       << "-" << architectureToString(architecture_)
       << "-" << platformToString(platform_);
    return ss.str();
}

// DefaultToolchainPolicy implementation
bool DefaultToolchainPolicy::canInstall(const Toolchain& toolchain) const
{
    // Can install if not already installed
    return !toolchain.isInstalled();
}

bool DefaultToolchainPolicy::canSelect(const Toolchain& toolchain) const
{
    // Can select if installed and not already selected
    return toolchain.isInstalled() && !toolchain.isSelected();
}

bool DefaultToolchainPolicy::canRemove(const Toolchain& toolchain) const
{
    // Can remove if installed and not currently selected
    return toolchain.isInstalled() && !toolchain.isSelected();
}

// ToolchainSpecification implementation
ToolchainSpecification ToolchainSpecification::parse(const std::string& spec)
{
    ToolchainSpecification result;

    // Parse format: name@version or name
    size_t atPos = spec.find('@');
    if (atPos != std::string::npos) {
        result.name = spec.substr(0, atPos);
        result.version = spec.substr(atPos + 1);
    } else {
        result.name = spec;
        result.version = "latest";
    }

    // Auto-detect current platform if not specified
    result.architecture = getCurrentArchitecture();
    result.platform = getCurrentPlatform();

    return result;
}

// Helper functions
std::string architectureToString(const Architecture arch)
{
    switch (arch) {
        case Architecture::X86_64:
            return "x86_64";
        case Architecture::ARM64:
            return "aarch64";
        default:
            return "unknown";
    }
}

std::string platformToString(const Platform platform)
{
    switch (platform) {
        case Platform::Linux:
            return "linux";
        case Platform::Darwin:
            return "darwin";
        case Platform::Windows:
            return "windows";
        default:
            return "unknown";
    }
}

Architecture stringToArchitecture(const std::string& str)
{
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "x86_64" || lower == "amd64") {
        return Architecture::X86_64;
    } else if (lower == "aarch64" || lower == "arm64") {
        return Architecture::ARM64;
    }
    return Architecture::Unknown;
}

Platform stringToPlatform(const std::string& str)
{
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "linux") {
        return Platform::Linux;
    } else if (lower == "darwin" || lower == "macos" || lower == "osx") {
        return Platform::Darwin;
    } else if (lower == "windows" || lower == "win32" || lower == "win64") {
        return Platform::Windows;
    }
    return Platform::Unknown;
}

Architecture getCurrentArchitecture()
{
#if defined(__x86_64__) || defined(_M_X64)
    return Architecture::X86_64;
#elif defined(__aarch64__) || defined(_M_ARM64)
    return Architecture::ARM64;
#else
    return Architecture::Unknown;
#endif
}

Platform getCurrentPlatform()
{
#ifdef _WIN32
    return Platform::Windows;
#elif defined(__APPLE__)
    return Platform::Darwin;
#elif defined(__linux__)
    return Platform::Linux;
#else
    return Platform::Unknown;
#endif
}

} // namespace scrap::toolchain::model
