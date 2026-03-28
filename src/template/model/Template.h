#pragma once

#include "TemplateError.h"
#include <dross/type/error.h>
#include <expected>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace scrap::template_system::model {

/**
 * @brief Template variable definition
 */
struct TemplateVariable {
    enum class Type {
        String,
        Boolean,
        Number
    };

    std::string name;
    std::string prompt;
    std::optional<std::string> defaultValue;
    Type type = Type::String;
    bool required = false;
    std::vector<std::string> choices;       // For enum-like variables
    std::optional<std::string> validation;  // Regex pattern
    std::optional<std::string> transform;   // Variable transformation

    TemplateVariable(const std::string& n, const std::string& p);
};

/**
 * @brief Template source information
 */
enum class TemplateSourceType {
    Official,
    Git,
    Local
};

struct TemplateSource {
    std::string name;
    TemplateSourceType type;
    std::optional<std::string> url;             // For Git sources
    std::optional<std::filesystem::path> path;  // For Local sources
    std::string branch = "main";
    bool autoUpdate = true;

    TemplateSource(const std::string& n, TemplateSourceType t);
};

/**
 * @brief Template requirements specification
 */
struct TemplateRequirements {
    std::vector<std::string> features;
    std::optional<std::string> toolchain;
    std::optional<std::string> minCppStandard;
    std::optional<std::string> minScrapVersion;

    bool isCompatible() const;
};

/**
 * @brief Template metadata and content
 */
class Template {
public:
    Template(const std::string& name, const std::filesystem::path& path, const TemplateSource& source);

    // Basic information
    const std::string& name() const;
    const std::string& version() const;
    const std::string& description() const;
    const std::string& author() const;
    const std::string& license() const;
    const std::vector<std::string>& tags() const;

    // Paths
    const std::filesystem::path& path() const;
    const TemplateSource& source() const;

    // Variables and requirements
    const std::vector<TemplateVariable>& variables() const;
    const TemplateRequirements& requirements() const;
    const std::map<std::string, std::string>& defaultDependencies() const;

    // Setters (used during loading)
    void setVersion(const std::string& version);
    void setDescription(const std::string& description);
    void setAuthor(const std::string& author);
    void setLicense(const std::string& license);
    void addTag(const std::string& tag);
    void addVariable(const TemplateVariable& variable);
    void setRequirements(const TemplateRequirements& requirements);
    void addDefaultDependency(const std::string& name, const std::string& version);

    // Validation
    bool isValid() const;
    std::vector<std::string> validate() const;

    // Template file operations
    std::vector<std::filesystem::path> templateFiles() const;
    bool hasTemplateFile(const std::string& filename) const;
    std::string fullName() const;

private:
    // Basic metadata
    std::string name_;
    std::string version_ = "1.0.0";
    std::string description_;
    std::string author_;
    std::string license_;
    std::vector<std::string> tags_;

    // Paths and source
    std::filesystem::path path_;
    TemplateSource source_;

    // Template configuration
    std::vector<TemplateVariable> variables_;
    TemplateRequirements requirements_;
    std::map<std::string, std::string> defaultDependencies_;

    void loadMetadata();
};

/**
 * @brief Collection of template variables with their resolved values
 */
class VariableMap {
public:
    void set(const std::string& name, const std::string& value);
    std::optional<std::string> get(const std::string& name) const;
    bool has(const std::string& name) const;

    // Standard variables (always available)
    void setStandardVariables(const std::string& projectName, const std::string& projectVersion = "0.1.0");

    const std::map<std::string, std::string>& all() const;

    // Variable transformation
    std::string applyTransform(const std::string& value, const std::string& transform) const;

private:
    std::map<std::string, std::string> variables_;

    std::string currentYear() const;
    std::string currentDate() const;
    std::string currentUser() const;
};

// Helper functions
std::string templateSourceTypeToString(TemplateSourceType type);
[[nodiscard]] std::expected<TemplateSourceType, dross::error>
stringToTemplateSourceType(const std::string& str) noexcept;

}  // namespace scrap::template_system::model
