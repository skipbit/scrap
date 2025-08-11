#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <optional>
#include <map>

namespace scrap::project::model {

/**
 * @brief Value object for project name
 */
class ProjectName {
public:
    explicit ProjectName(const std::string& value);
    const std::string& value() const { return value_; }
    std::string toString() const { return value_; }

    bool operator==(const ProjectName& other) const { return value_ == other.value_; }

private:
    std::string value_;
    void validate() const;
};

/**
 * @brief Enumeration for project types
 */
enum class ProjectType {
    Application,  // Executable application
    Library,      // Static/shared library
    Unknown
};

/**
 * @brief Enumeration for build modes
 */
enum class BuildMode {
    Debug,
    Release,
    RelWithDebInfo,
    MinSizeRel
};

/**
 * @brief Value object for version
 */
class Version {
public:
    Version(int major = 0, int minor = 1, int patch = 0);
    explicit Version(const std::string& versionStr);

    int major() const { return major_; }
    int minor() const { return minor_; }
    int patch() const { return patch_; }

    std::string toString() const;
    bool operator==(const Version& other) const;

private:
    int major_, minor_, patch_;
};

/**
 * @brief Value object for dependency specification
 */
class Dependency {
public:
    Dependency(const std::string& name, const std::string& version,
               const std::vector<std::string>& features = {});

    const std::string& name() const { return name_; }
    const std::string& version() const { return version_; }
    const std::vector<std::string>& features() const { return features_; }

    std::string toString() const;

private:
    std::string name_;
    std::string version_;
    std::vector<std::string> features_;
};

/**
 * @brief Build configuration settings
 */
class BuildConfiguration {
public:
    BuildConfiguration();

    // Getters
    BuildMode mode() const { return mode_; }
    const std::string& cppStandard() const { return cppStandard_; }
    const std::vector<std::string>& compilerFlags() const { return compilerFlags_; }
    const std::vector<std::string>& linkerFlags() const { return linkerFlags_; }
    const std::map<std::string, std::string>& definitions() const { return definitions_; }

    // Setters
    void setMode(BuildMode mode) { mode_ = mode; }
    void setCppStandard(const std::string& standard) { cppStandard_ = standard; }
    void addCompilerFlag(const std::string& flag) { compilerFlags_.push_back(flag); }
    void addLinkerFlag(const std::string& flag) { linkerFlags_.push_back(flag); }
    void addDefinition(const std::string& key, const std::string& value) {
        definitions_[key] = value;
    }

private:
    BuildMode mode_;
    std::string cppStandard_;
    std::vector<std::string> compilerFlags_;
    std::vector<std::string> linkerFlags_;
    std::map<std::string, std::string> definitions_;
};

/**
 * @brief Build result information
 */
struct BuildResult {
    enum class Status {
        Success,
        Failed,
        Cancelled
    };

    Status status;
    std::string message;
    std::chrono::milliseconds duration;
    std::vector<std::filesystem::path> artifacts;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;

    static BuildResult success(const std::string& message = "",
                               std::chrono::milliseconds duration = {},
                               const std::vector<std::filesystem::path>& artifacts = {});

    static BuildResult failed(const std::string& message,
                              const std::vector<std::string>& errors = {});

    bool isSuccess() const { return status == Status::Success; }
};

/**
 * @brief Main project entity
 */
class Project {
public:
    Project(const ProjectName& name,
            ProjectType type,
            const Version& version = Version());

    // Getters
    const ProjectName& name() const { return name_; }
    ProjectType type() const { return type_; }
    const Version& version() const { return version_; }
    const std::optional<std::filesystem::path>& path() const { return path_; }
    const BuildConfiguration& buildConfig() const { return buildConfig_; }
    const std::vector<Dependency>& dependencies() const { return dependencies_; }
    const std::optional<std::string>& toolchainRequirement() const { return toolchainRequirement_; }

    // Setters
    void setPath(const std::filesystem::path& path) { path_ = path; }
    void setBuildConfig(const BuildConfiguration& config) { buildConfig_ = config; }
    void addDependency(const Dependency& dependency) { dependencies_.push_back(dependency); }
    void setToolchainRequirement(const std::string& requirement) {
        toolchainRequirement_ = requirement;
    }

    // Business logic
    bool isApplication() const { return type_ == ProjectType::Application; }
    bool isLibrary() const { return type_ == ProjectType::Library; }
    std::string fullName() const;
    std::filesystem::path buildDirectory(BuildMode mode) const;

private:
    ProjectName name_;
    ProjectType type_;
    Version version_;
    std::optional<std::filesystem::path> path_;
    BuildConfiguration buildConfig_;
    std::vector<Dependency> dependencies_;
    std::optional<std::string> toolchainRequirement_;
};

/**
 * @brief Project creation specification
 */
struct ProjectSpecification {
    std::string name;
    ProjectType type;
    std::optional<std::string> templateName;
    std::optional<std::filesystem::path> targetPath;
    std::optional<std::string> cppStandard;
    std::vector<std::string> initialDependencies;

    static ProjectSpecification parse(const std::vector<std::string>& args);
};

/**
 * @brief Build options for build command
 */
struct BuildOptions {
    BuildMode mode = BuildMode::Debug;
    bool verbose = false;
    bool clean = false;
    std::optional<std::string> target;
    int parallelJobs = 0;  // 0 = auto-detect

    static BuildOptions parse(const std::vector<std::string>& args);
};

/**
 * @brief Run options for run command
 */
struct RunOptions {
    std::vector<std::string> arguments;
    std::optional<std::filesystem::path> workingDirectory;

    static RunOptions parse(const std::vector<std::string>& args);
};

// Helper functions
std::string projectTypeToString(ProjectType type);
ProjectType stringToProjectType(const std::string& str);
std::string buildModeToString(BuildMode mode);
BuildMode stringToBuildMode(const std::string& str);

} // namespace scrap::project::model
