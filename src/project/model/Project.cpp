#include "Project.h"
#include <stdexcept>
#include <sstream>
#include <regex>
#include <algorithm>
#include <chrono>

namespace scrap::project::model {

// ProjectName implementation
ProjectName::ProjectName(const std::string& value) : value_(value) {
    validate();
}

void ProjectName::validate() const {
    if (value_.empty()) {
        throw std::invalid_argument("Project name cannot be empty");
    }

    // Check for valid C++ identifier pattern
    std::regex validName("^[a-zA-Z_][a-zA-Z0-9_]*$");
    if (!std::regex_match(value_, validName)) {
        throw std::invalid_argument("Project name must be a valid C++ identifier: " + value_);
    }

    // Check for reserved keywords
    static const std::vector<std::string> reservedKeywords = {
        "class", "struct", "namespace", "template", "typename", "const", "static",
        "int", "char", "bool", "void", "return", "if", "else", "for", "while"
    };

    if (std::find(reservedKeywords.begin(), reservedKeywords.end(), value_) != reservedKeywords.end()) {
        throw std::invalid_argument("Project name cannot be a C++ reserved keyword: " + value_);
    }
}

// Version implementation
Version::Version(int major, int minor, int patch)
    : major_(major), minor_(minor), patch_(patch) {
    if (major < 0 || minor < 0 || patch < 0) {
        throw std::invalid_argument("Version components cannot be negative");
    }
}

Version::Version(const std::string& versionStr) {
    std::regex versionPattern(R"(^(\d+)\.(\d+)\.(\d+)$)");
    std::smatch match;

    if (!std::regex_match(versionStr, match, versionPattern)) {
        throw std::invalid_argument("Invalid version format, expected X.Y.Z: " + versionStr);
    }

    major_ = std::stoi(match[1]);
    minor_ = std::stoi(match[2]);
    patch_ = std::stoi(match[3]);
}

std::string Version::toString() const {
    return std::to_string(major_) + "." + std::to_string(minor_) + "." + std::to_string(patch_);
}

bool Version::operator==(const Version& other) const {
    return major_ == other.major_ && minor_ == other.minor_ && patch_ == other.patch_;
}

// Dependency implementation
Dependency::Dependency(const std::string& name, const std::string& version,
                       const std::vector<std::string>& features)
    : name_(name), version_(version), features_(features) {
    if (name.empty()) {
        throw std::invalid_argument("Dependency name cannot be empty");
    }
    if (version.empty()) {
        throw std::invalid_argument("Dependency version cannot be empty");
    }
}

std::string Dependency::toString() const {
    std::stringstream ss;
    ss << name_ << "@" << version_;
    if (!features_.empty()) {
        ss << " [";
        for (size_t i = 0; i < features_.size(); ++i) {
            if (i > 0) ss << ", ";
            ss << features_[i];
        }
        ss << "]";
    }
    return ss.str();
}

// BuildConfiguration implementation
BuildConfiguration::BuildConfiguration()
    : mode_(BuildMode::Debug), cppStandard_("23") {
}

// BuildResult implementation
BuildResult BuildResult::success(const std::string& message,
                                 std::chrono::milliseconds duration,
                                 const std::vector<std::filesystem::path>& artifacts) {
    BuildResult result;
    result.status = Status::Success;
    result.message = message;
    result.duration = duration;
    result.artifacts = artifacts;
    return result;
}

BuildResult BuildResult::failed(const std::string& message,
                                const std::vector<std::string>& errors) {
    BuildResult result;
    result.status = Status::Failed;
    result.message = message;
    result.errors = errors;
    return result;
}

// Project implementation
Project::Project(const ProjectName& name, ProjectType type, const Version& version)
    : name_(name), type_(type), version_(version) {
}

std::string Project::getFullName() const {
    return name_.toString() + " v" + version_.toString();
}

std::filesystem::path Project::getBuildDirectory(BuildMode mode) const {
    if (!path_) {
        return "build";
    }

    std::string modeStr = buildModeToString(mode);
    std::transform(modeStr.begin(), modeStr.end(), modeStr.begin(), ::tolower);

    return *path_ / "build" / modeStr;
}

// ProjectSpecification implementation
ProjectSpecification ProjectSpecification::parse(const std::vector<std::string>& args) {
    ProjectSpecification spec;

    if (args.empty()) {
        throw std::invalid_argument("Project name is required");
    }

    spec.name = args[0];
    spec.type = ProjectType::Application; // default

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
        } else if (!arg.starts_with("--")) {
            // Treat as initial dependency
            spec.initialDependencies.push_back(arg);
        }
    }

    return spec;
}

// BuildOptions implementation
BuildOptions BuildOptions::parse(const std::vector<std::string>& args) {
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
            options.parallelJobs = std::stoi(arg.substr(2));
        }
    }

    return options;
}

// RunOptions implementation
RunOptions RunOptions::parse(const std::vector<std::string>& args) {
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
std::string projectTypeToString(ProjectType type) {
    switch (type) {
        case ProjectType::Application:
            return "application";
        case ProjectType::Library:
            return "library";
        default:
            return "unknown";
    }
}

ProjectType stringToProjectType(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "app" || lower == "application" || lower == "exe") {
        return ProjectType::Application;
    } else if (lower == "lib" || lower == "library") {
        return ProjectType::Library;
    }
    return ProjectType::Unknown;
}

std::string buildModeToString(BuildMode mode) {
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

BuildMode stringToBuildMode(const std::string& str) {
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

} // namespace scrap::project::model
