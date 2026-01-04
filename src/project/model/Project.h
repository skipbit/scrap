#pragma once

#include "ProjectError.h"
#include <dross/type/error.h>
#include <expected>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace scrap::Project::Model {

/**
 * @brief Value object for project name
 */
class ProjectName {
public:
    /**
     * @brief Create ProjectName with validation
     * @param value Project name to validate
     * @return ProjectName if valid, error otherwise
     */
    [[nodiscard]] static std::expected<ProjectName, dross::error> create(const std::string& value) noexcept;

    const std::string& value() const;
    std::string toString() const;

    bool operator==(const ProjectName& other) const;

private:
    std::string value_;

    // Private constructor - use create() factory method
    explicit ProjectName(std::string value);
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
    /**
     * @brief Create default version (0.1.0)
     */
    [[nodiscard]] static Version createDefault() noexcept;

    /**
     * @brief Create Version with validation
     * @param major Major version number
     * @param minor Minor version number
     * @param patch Patch version number
     * @return Version if valid, error otherwise
     */
    [[nodiscard]] static std::expected<Version, dross::error> create(int major, int minor, int patch) noexcept;

    /**
     * @brief Parse Version from string (X.Y.Z format)
     * @param versionStr Version string to parse
     * @return Version if valid, error otherwise
     */
    [[nodiscard]] static std::expected<Version, dross::error> parse(const std::string& versionStr) noexcept;

    int major() const;
    int minor() const;
    int patch() const;

    std::string toString() const;
    bool operator==(const Version& other) const;

private:
    int major_, minor_, patch_;

    // Private constructor - use create() or parse() factory methods
    Version(int major, int minor, int patch);
};

/**
 * @brief Value object for dependency specification
 */
class Dependency {
public:
    /**
     * @brief Create Dependency with validation
     * @param name Dependency name
     * @param version Dependency version
     * @param features Optional feature flags
     * @return Dependency if valid, error otherwise
     */
    [[nodiscard]] static std::expected<Dependency, dross::error>
    create(const std::string& name, const std::string& version, const std::vector<std::string>& features = {}) noexcept;

    const std::string& name() const;
    const std::string& version() const;
    const std::vector<std::string>& features() const;

    std::string toString() const;

private:
    std::string name_;
    std::string version_;
    std::vector<std::string> features_;

    // Private constructor - use create() factory method
    Dependency(std::string name, std::string version, std::vector<std::string> features);
};

/**
 * @brief Build configuration settings
 */
class BuildConfiguration {
public:
    BuildConfiguration();

    // Getters
    BuildMode mode() const;
    const std::string& cppStandard() const;
    const std::vector<std::string>& compilerFlags() const;
    const std::vector<std::string>& linkerFlags() const;
    const std::map<std::string, std::string>& definitions() const;

    // Setters
    void setMode(BuildMode mode);
    void setCppStandard(const std::string& standard);
    void addCompilerFlag(const std::string& flag);
    void addLinkerFlag(const std::string& flag);
    void addDefinition(const std::string& key, const std::string& value);

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

    static BuildResult failed(const std::string& message, const std::vector<std::string>& errors = {});

    bool isSuccess() const;
};

/**
 * @brief Main project entity
 */
class Project {
public:
    Project(const ProjectName& name, ProjectType type, const Version& version = Version::createDefault());

    // Getters
    const ProjectName& name() const;
    ProjectType type() const;
    const Version& version() const;
    const std::optional<std::filesystem::path>& path() const;
    const BuildConfiguration& buildConfig() const;
    const std::vector<Dependency>& dependencies() const;
    const std::optional<std::string>& toolchainRequirement() const;

    // Setters
    void setPath(const std::filesystem::path& path);
    void setBuildConfig(const BuildConfiguration& config);
    void addDependency(const Dependency& dependency);
    void setToolchainRequirement(const std::string& requirement);

    // Business logic
    bool isApplication() const;
    bool isLibrary() const;
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

    [[nodiscard]] static std::expected<ProjectSpecification, dross::error>
    parse(const std::vector<std::string>& args) noexcept;
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

    [[nodiscard]] static std::expected<BuildOptions, dross::error> parse(const std::vector<std::string>& args) noexcept;
};

/**
 * @brief Run options for run command
 */
struct RunOptions {
    std::vector<std::string> arguments;
    std::optional<std::filesystem::path> workingDirectory;

    [[nodiscard]] static std::expected<RunOptions, dross::error> parse(const std::vector<std::string>& args) noexcept;
};

// Helper functions
std::string projectTypeToString(ProjectType type);
ProjectType stringToProjectType(const std::string& str);
std::string buildModeToString(BuildMode mode);
BuildMode stringToBuildMode(const std::string& str);

}  // namespace scrap::Project::Model
