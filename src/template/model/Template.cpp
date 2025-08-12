#include "Template.h"
#include <regex>
#include <chrono>
#include <ctime>

namespace scrap::template_system::model {

// TemplateVariable implementation
TemplateVariable::TemplateVariable(const std::string& n, const std::string& p)
    : name(n), prompt(p)
{
}

// TemplateSource implementation
TemplateSource::TemplateSource(const std::string& n, TemplateSourceType t)
    : name(n), type(t)
{
}

bool TemplateRequirements::isCompatible() const
{
    // For now, just return true
    // In future, implement actual compatibility checking
    return true;
}

Template::Template(const std::string& name,
                   const std::filesystem::path& path,
                   const TemplateSource& source)
    : name_(name), path_(path), source_(source)
{
    loadMetadata();
}

// Template getters
const std::string& Template::name() const
{
    return name_;
}

const std::string& Template::version() const
{
    return version_;
}

const std::string& Template::description() const
{
    return description_;
}

const std::string& Template::author() const
{
    return author_;
}

const std::string& Template::license() const
{
    return license_;
}

const std::vector<std::string>& Template::tags() const
{
    return tags_;
}

const std::filesystem::path& Template::path() const
{
    return path_;
}

const TemplateSource& Template::source() const
{
    return source_;
}

const std::vector<TemplateVariable>& Template::variables() const
{
    return variables_;
}

const TemplateRequirements& Template::requirements() const
{
    return requirements_;
}

const std::map<std::string, std::string>& Template::defaultDependencies() const
{
    return defaultDependencies_;
}

// Template setters
void Template::setVersion(const std::string& version)
{
    version_ = version;
}

void Template::setDescription(const std::string& description)
{
    description_ = description;
}

void Template::setAuthor(const std::string& author)
{
    author_ = author;
}

void Template::setLicense(const std::string& license)
{
    license_ = license;
}

void Template::addTag(const std::string& tag)
{
    tags_.push_back(tag);
}

void Template::addVariable(const TemplateVariable& variable)
{
    variables_.push_back(variable);
}

void Template::setRequirements(const TemplateRequirements& requirements)
{
    requirements_ = requirements;
}

void Template::addDefaultDependency(const std::string& name, const std::string& version)
{
    defaultDependencies_[name] = version;
}

bool Template::isValid() const
{
    return validate().empty();
}

std::vector<std::string> Template::validate() const
{
    std::vector<std::string> errors;

    if (name_.empty()) {
        errors.push_back("Template name cannot be empty");
    }

    if (!std::filesystem::exists(path_)) {
        errors.push_back("Template path does not exist: " + path_.string());
    }

    // Check for required files
    auto templateToml = path_ / "template.toml";
    if (!std::filesystem::exists(templateToml)) {
        errors.push_back("template.toml not found in template directory");
    }

    // Validate variable names (should be valid identifiers)
    for (const auto& var : variables_) {
        if (var.name.empty()) {
            errors.push_back("Template variable name cannot be empty");
            continue;
        }

        // Check if variable name is a valid identifier
        std::regex identifierPattern("^[a-zA-Z_][a-zA-Z0-9_]*$");
        if (!std::regex_match(var.name, identifierPattern)) {
            errors.push_back("Invalid variable name: " + var.name);
        }
    }

    return errors;
}

std::vector<std::filesystem::path> Template::templateFiles() const
{
    std::vector<std::filesystem::path> files;

    if (!std::filesystem::exists(path_)) {
        return files;
    }

    // Recursively collect all files except template.toml and .scrap-ignore
    std::error_code ec;
    for (auto& entry : std::filesystem::recursive_directory_iterator(path_, ec)) {
        if (ec) continue; // Skip errors

        if (entry.is_regular_file()) {
            auto relativePath = std::filesystem::relative(entry.path(), path_);
            auto filename = relativePath.filename().string();

            // Skip metadata files
            if (filename == "template.toml" || filename == ".scrap-ignore") {
                continue;
            }

            files.push_back(relativePath);
        }
    }

    return files;
}

bool Template::hasTemplateFile(const std::string& filename) const
{
    auto filePath = path_ / filename;
    return std::filesystem::exists(filePath);
}

std::string Template::fullName() const
{
    return source_.name + "/" + name_;
}

void Template::loadMetadata()
{
    auto templateToml = path_ / "template.toml";

    if (!std::filesystem::exists(templateToml)) {
        // If no template.toml exists, use defaults based on directory name
        return;
    }

    // TODO: Implement TOML parsing when dross TOML support is ready
    // For now, just set some defaults
    description_ = "Template: " + name_;
    author_ = "Unknown";
    license_ = "MIT";
}

void VariableMap::set(const std::string& name, const std::string& value)
{
    variables_[name] = value;
}

std::optional<std::string> VariableMap::get(const std::string& name) const
{
    auto it = variables_.find(name);
    if (it != variables_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool VariableMap::has(const std::string& name) const
{
    return variables_.find(name) != variables_.end();
}

const std::map<std::string, std::string>& VariableMap::all() const
{
    return variables_;
}

void VariableMap::setStandardVariables(const std::string& projectName,
                                       const std::string& projectVersion)
{
    set("name", projectName);
    set("version", projectVersion);
    set("year", currentYear());
    set("date", currentDate());
    set("author", currentUser());
    set("scrap_version", "0.0.1"); // TODO: Get actual scrap version
}

std::string VariableMap::applyTransform(const std::string& value,
                                        const std::string& transform) const
{
    if (transform == "lower_case") {
        std::string result = value;
        std::transform(result.begin(), result.end(), result.begin(), ::tolower);
        return result;
    }

    if (transform == "UPPER_CASE") {
        std::string result = value;
        std::transform(result.begin(), result.end(), result.begin(), ::toupper);
        return result;
    }

    if (transform == "snake_case") {
        std::string result;
        bool prevWasUpper = false;

        for (size_t i = 0; i < value.length(); ++i) {
            char c = value[i];

            if (std::isupper(c)) {
                if (i > 0 && !prevWasUpper) {
                    result += '_';
                }
                result += std::tolower(c);
                prevWasUpper = true;
            } else if (c == '-' || c == ' ') {
                result += '_';
                prevWasUpper = false;
            } else {
                result += c;
                prevWasUpper = false;
            }
        }

        return result;
    }

    if (transform == "PascalCase") {
        std::string result;
        bool nextUpper = true;

        for (char c : value) {
            if (c == '_' || c == '-' || c == ' ') {
                nextUpper = true;
            } else if (nextUpper) {
                result += std::toupper(c);
                nextUpper = false;
            } else {
                result += std::tolower(c);
            }
        }

        return result;
    }

    if (transform == "camelCase") {
        std::string pascalCase = applyTransform(value, "PascalCase");
        if (!pascalCase.empty()) {
            pascalCase[0] = std::tolower(pascalCase[0]);
        }
        return pascalCase;
    }

    if (transform == "kebab-case") {
        std::string snakeCase = applyTransform(value, "snake_case");
        std::replace(snakeCase.begin(), snakeCase.end(), '_', '-');
        return snakeCase;
    }

    // Unknown transform, return original value
    return value;
}

std::string VariableMap::currentYear() const
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto tm = *std::localtime(&time_t);
    return std::to_string(1900 + tm.tm_year);
}

std::string VariableMap::currentDate() const
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto tm = *std::localtime(&time_t);

    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &tm);
    return std::string(buffer);
}

std::string VariableMap::currentUser() const
{
    const char* user = std::getenv("USER");
    if (!user) {
        user = std::getenv("USERNAME"); // Windows
    }
    return user ? std::string(user) : "unknown";
}

std::string templateSourceTypeToString(TemplateSourceType type)
{
    switch (type) {
        case TemplateSourceType::Official: return "official";
        case TemplateSourceType::Git: return "git";
        case TemplateSourceType::Local: return "local";
    }
    return "unknown";
}

TemplateSourceType stringToTemplateSourceType(const std::string& str)
{
    if (str == "official") return TemplateSourceType::Official;
    if (str == "git") return TemplateSourceType::Git;
    if (str == "local") return TemplateSourceType::Local;

    throw std::invalid_argument("Invalid template source type: " + str);
}

} // namespace scrap::template_system::model
