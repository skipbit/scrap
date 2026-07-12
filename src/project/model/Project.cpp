#include "Project.h"
#include "ProjectError.h"
#include <algorithm>
#include <chrono>
#include <dross/type/error.h>
#include <expected>
#include <regex>
#include <sstream>

namespace scrap::Project::Model {

// ProjectName implementation
ProjectName::ProjectName(std::string value)
    : value_(std::move(value))
{
}

std::expected<ProjectName, dross::error> ProjectName::create(const std::string& value) noexcept
{
    if (value.empty()) {
        auto errorCode = make_error_code(ProjectNameError::Empty);
        return std::unexpected(dross::error{errorCode.value(), errorCode.category()});
    }

    // Check for valid C++ identifier pattern
    static const std::regex validName("^[a-zA-Z_][a-zA-Z0-9_]*$");
    if (! std::regex_match(value, validName)) {
        auto errorCode = make_error_code(ProjectNameError::InvalidIdentifier);
        return std::unexpected(dross::error{errorCode.value(), errorCode.category()});
    }

    // Check for reserved keywords
    static const std::vector<std::string> reservedKeywords = {"class",
                                                              "struct",
                                                              "namespace",
                                                              "template",
                                                              "typename",
                                                              "const",
                                                              "static",
                                                              "int",
                                                              "char",
                                                              "bool",
                                                              "void",
                                                              "return",
                                                              "if",
                                                              "else",
                                                              "for",
                                                              "while"};

    if (std::find(reservedKeywords.begin(), reservedKeywords.end(), value) != reservedKeywords.end()) {
        auto errorCode = make_error_code(ProjectNameError::ReservedKeyword);
        return std::unexpected(dross::error{errorCode.value(), errorCode.category()});
    }

    return ProjectName{value};
}

const std::string& ProjectName::value() const
{
    return value_;
}

std::string ProjectName::toString() const
{
    return value_;
}

bool ProjectName::operator==(const ProjectName& other) const
{
    return value_ == other.value_;
}

// Version implementation
Version::Version(int major, int minor, int patch)
    : major_(major), minor_(minor), patch_(patch)
{
}

Version Version::createDefault() noexcept
{
    return Version{0, 1, 0};
}

std::expected<Version, dross::error> Version::create(int major, int minor, int patch) noexcept
{
    if (major < 0 || minor < 0 || patch < 0) {
        auto errorCode = make_error_code(VersionError::NegativeComponent);
        return std::unexpected(dross::error{errorCode.value(), errorCode.category()});
    }

    return Version{major, minor, patch};
}

std::expected<Version, dross::error> Version::parse(const std::string& versionStr) noexcept
{
    static const std::regex versionPattern(R"(^(\d+)\.(\d+)\.(\d+)$)");
    std::smatch match;

    if (! std::regex_match(versionStr, match, versionPattern)) {
        auto errorCode = make_error_code(VersionError::InvalidFormat);
        return std::unexpected(dross::error{errorCode.value(), errorCode.category()});
    }

    // Parse version components
    // Note: std::stoi could theoretically throw, but the regex has validated
    // that we have valid digits, so we wrap in try-catch for noexcept guarantee
    try {
        const int major = std::stoi(match[1]);
        const int minor = std::stoi(match[2]);
        const int patch = std::stoi(match[3]);
        return Version{major, minor, patch};
    } catch (...) {
        // This should never happen due to regex validation, but handle for noexcept safety
        auto errorCode = make_error_code(VersionError::InvalidFormat);
        return std::unexpected(dross::error{errorCode.value(), errorCode.category()});
    }
}

std::string Version::toString() const
{
    return std::to_string(major_) + "." + std::to_string(minor_) + "." + std::to_string(patch_);
}

bool Version::operator==(const Version& other) const
{
    return major_ == other.major_ && minor_ == other.minor_ && patch_ == other.patch_;
}

int Version::major() const
{
    return major_;
}

int Version::minor() const
{
    return minor_;
}

int Version::patch() const
{
    return patch_;
}

// Dependency implementation
Dependency::Dependency(std::string name, std::string version, std::vector<std::string> features)
    : name_(std::move(name)), version_(std::move(version)), features_(std::move(features))
{
}

std::expected<Dependency, dross::error> Dependency::create(const std::string& name,
                                                           const std::string& version,
                                                           const std::vector<std::string>& features) noexcept
{
    if (name.empty()) {
        auto errorCode = make_error_code(DependencyError::EmptyName);
        return std::unexpected(dross::error{errorCode.value(), errorCode.category()});
    }
    if (version.empty()) {
        auto errorCode = make_error_code(DependencyError::EmptyVersion);
        return std::unexpected(dross::error{errorCode.value(), errorCode.category()});
    }

    return Dependency{name, version, features};
}

const std::string& Dependency::name() const
{
    return name_;
}

const std::string& Dependency::version() const
{
    return version_;
}

const std::vector<std::string>& Dependency::features() const
{
    return features_;
}

std::string Dependency::toString() const
{
    std::stringstream ss;
    ss << name_ << "@" << version_;
    if (! features_.empty()) {
        ss << " [";
        for (size_t i = 0; i < features_.size(); ++i) {
            if (i > 0)
                ss << ", ";
            ss << features_[i];
        }
        ss << "]";
    }
    return ss.str();
}

// BuildConfiguration implementation
BuildConfiguration::BuildConfiguration()
    : mode_(BuildMode::Debug), cppStandard_("23")
{
}

// BuildConfiguration implementation
BuildMode BuildConfiguration::mode() const
{
    return mode_;
}

const std::string& BuildConfiguration::cppStandard() const
{
    return cppStandard_;
}

const std::vector<std::string>& BuildConfiguration::compilerFlags() const
{
    return compilerFlags_;
}

const std::vector<std::string>& BuildConfiguration::linkerFlags() const
{
    return linkerFlags_;
}

const std::map<std::string, std::string>& BuildConfiguration::definitions() const
{
    return definitions_;
}

void BuildConfiguration::setMode(BuildMode mode)
{
    mode_ = mode;
}

void BuildConfiguration::setCppStandard(const std::string& standard)
{
    cppStandard_ = standard;
}

void BuildConfiguration::addCompilerFlag(const std::string& flag)
{
    compilerFlags_.push_back(flag);
}

void BuildConfiguration::addLinkerFlag(const std::string& flag)
{
    linkerFlags_.push_back(flag);
}

void BuildConfiguration::addDefinition(const std::string& key, const std::string& value)
{
    definitions_[key] = value;
}

// BuildResult implementation
BuildResult BuildResult::success(const std::string& message,
                                 std::chrono::milliseconds duration,
                                 const std::vector<std::filesystem::path>& artifacts)
{
    BuildResult result;
    result.status = Status::Success;
    result.message = message;
    result.duration = duration;
    result.artifacts = artifacts;
    return result;
}

BuildResult BuildResult::failed(const std::string& message, const std::vector<std::string>& errors)
{
    BuildResult result;
    result.status = Status::Failed;
    result.message = message;
    result.errors = errors;
    return result;
}

bool BuildResult::isSuccess() const
{
    return status == Status::Success;
}

// Project implementation
Project::Project(const ProjectName& name, ProjectType type, const Version& version)
    : name_(name), type_(type), version_(version)
{
}

const ProjectName& Project::name() const
{
    return name_;
}

ProjectType Project::type() const
{
    return type_;
}

const Version& Project::version() const
{
    return version_;
}

const std::optional<std::filesystem::path>& Project::path() const
{
    return path_;
}

const BuildConfiguration& Project::buildConfig() const
{
    return buildConfig_;
}

const std::vector<Dependency>& Project::dependencies() const
{
    return dependencies_;
}

const std::optional<std::string>& Project::toolchainRequirement() const
{
    return toolchainRequirement_;
}

void Project::setPath(const std::filesystem::path& path)
{
    path_ = path;
}

void Project::setBuildConfig(const BuildConfiguration& config)
{
    buildConfig_ = config;
}

void Project::addDependency(const Dependency& dependency)
{
    dependencies_.push_back(dependency);
}

void Project::setToolchainRequirement(const std::string& requirement)
{
    toolchainRequirement_ = requirement;
}

bool Project::isApplication() const
{
    return type_ == ProjectType::Application;
}

bool Project::isLibrary() const
{
    return type_ == ProjectType::Library;
}

std::string Project::fullName() const
{
    return name_.toString() + " v" + version_.toString();
}

std::filesystem::path Project::buildDirectory(BuildMode mode) const
{
    if (! path_) {
        return "build";
    }

    std::string modeStr = buildModeToString(mode);
    std::transform(modeStr.begin(), modeStr.end(), modeStr.begin(), ::tolower);

    return *path_ / "build" / modeStr;
}

// ProjectSpecification implementation
std::expected<ProjectSpecification, dross::error>
ProjectSpecification::parse(const std::vector<std::string>& args) noexcept
{
    ProjectSpecification spec;

    if (args.empty()) {
        auto errorCode = make_error_code(ProjectSpecificationError::MissingProjectName);
        return std::unexpected(dross::error{errorCode.value(), errorCode.category()});
    }

    spec.name = args[0];
    spec.type = ProjectType::Application;  // default

    // Parse additional arguments
    for (size_t i = 1; i < args.size(); ++i) {
        const auto& arg = args[i];

        if (arg == "--type=app" || arg == "--type=application") {
            spec.type = ProjectType::Application;
        } else if (arg == "--type=lib" || arg == "--type=library") {
            spec.type = ProjectType::Library;
        } else if (arg.starts_with("--template=")) {
            spec.templateName = arg.substr(11);
        } else if (arg.starts_with("--path=")) {
            spec.targetPath = std::filesystem::path(arg.substr(7));
        } else if (arg.starts_with("--std=")) {
            spec.cppStandard = arg.substr(6);
        } else if (! arg.starts_with("--")) {
            // Treat as initial dependency
            spec.initialDependencies.push_back(arg);
        }
    }

    return spec;
}

// BuildOptions implementation
std::expected<BuildOptions, dross::error> BuildOptions::parse(const std::vector<std::string>& args) noexcept
{
    BuildOptions options;

    for (const auto& arg : args) {
        if (arg == "--release") {
            options.mode = BuildMode::Release;
        } else if (arg == "--debug") {
            options.mode = BuildMode::Debug;
        } else if (arg == "--verbose" || arg == "-v") {
            options.verbose = true;
        } else if (arg == "--clean") {
            options.clean = true;
        } else if (arg.starts_with("--target=")) {
            options.target = arg.substr(9);
        } else if (arg.starts_with("-j") && arg.length() > 2) {
            try {
                options.parallelJobs = std::stoi(arg.substr(2));
            } catch (const std::exception&) {
                auto errorCode = make_error_code(BuildOptionsError::InvalidParallelJobs);
                return std::unexpected(dross::error{errorCode.value(), errorCode.category()});
            }
        }
    }

    return options;
}

// RunOptions implementation
std::expected<RunOptions, dross::error> RunOptions::parse(const std::vector<std::string>& args) noexcept
{
    RunOptions options;

    bool foundSeparator = false;
    for (const auto& arg : args) {
        if (arg == "--") {
            foundSeparator = true;
            continue;
        }

        if (foundSeparator) {
            options.arguments.push_back(arg);
        } else if (arg.starts_with("--working-dir=")) {
            options.workingDirectory = std::filesystem::path(arg.substr(14));
        }
    }

    return options;
}

// Helper functions
std::string projectTypeToString(ProjectType type)
{
    switch (type) {
        case ProjectType::Application:
            return "application";
        case ProjectType::Library:
            return "library";
        default:
            return "unknown";
    }
}

ProjectType stringToProjectType(const std::string& str)
{
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "app" || lower == "application" || lower == "exe") {
        return ProjectType::Application;
    } else if (lower == "lib" || lower == "library") {
        return ProjectType::Library;
    }
    return ProjectType::Unknown;
}

std::string buildModeToString(BuildMode mode)
{
    switch (mode) {
        case BuildMode::Debug:
            return "Debug";
        case BuildMode::Release:
            return "Release";
        case BuildMode::RelWithDebInfo:
            return "RelWithDebInfo";
        case BuildMode::MinSizeRel:
            return "MinSizeRel";
        default:
            return "Debug";
    }
}

BuildMode stringToBuildMode(const std::string& str)
{
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "release" || lower == "rel") {
        return BuildMode::Release;
    } else if (lower == "debug" || lower == "dbg") {
        return BuildMode::Debug;
    } else if (lower == "relwithdebinfo" || lower == "relwithdbg") {
        return BuildMode::RelWithDebInfo;
    } else if (lower == "minsizerel" || lower == "minsize") {
        return BuildMode::MinSizeRel;
    }
    return BuildMode::Debug;
}

}  // namespace scrap::Project::Model
