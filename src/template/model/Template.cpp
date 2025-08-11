#include "Template.h"
#include <fstream>
#include <regex>
#include <chrono>
#include <ctime>

namespace scrap::template_system::model {

bool TemplateRequirements::isCompatible() const {
    // For now, just return true
    // In future, implement actual compatibility checking
    return true;
}

Template::Template(const std::string& name,
                   const std::filesystem::path& path,
                   const TemplateSource& source)
    : name_(name), path_(path), source_(source) {
    loadMetadata();
}

bool Template::isValid() const {
    return validate().empty();
}

std::vector<std::string> Template::validate() const {
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

std::vector<std::filesystem::path> Template::getTemplateFiles() const {
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

bool Template::hasTemplateFile(const std::string& filename) const {
    auto filePath = path_ / filename;
    return std::filesystem::exists(filePath);
}

std::string Template::getFullName() const {
    return source_.name + "/" + name_;
}

void Template::loadMetadata() {
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

void VariableMap::set(const std::string& name, const std::string& value) {
    variables_[name] = value;
}

std::optional<std::string> VariableMap::get(const std::string& name) const {
    auto it = variables_.find(name);
    if (it != variables_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool VariableMap::has(const std::string& name) const {
    return variables_.find(name) != variables_.end();
}

void VariableMap::setStandardVariables(const std::string& projectName,
                                       const std::string& projectVersion) {
    set("name", projectName);
    set("version", projectVersion);
    set("year", getCurrentYear());
    set("date", getCurrentDate());
    set("author", getCurrentUser());
    set("scrap_version", "0.0.1"); // TODO: Get actual scrap version
}

std::string VariableMap::applyTransform(const std::string& value,
                                        const std::string& transform) const {
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

std::string VariableMap::getCurrentYear() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto tm = *std::localtime(&time_t);
    return std::to_string(1900 + tm.tm_year);
}

std::string VariableMap::getCurrentDate() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto tm = *std::localtime(&time_t);

    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &tm);
    return std::string(buffer);
}

std::string VariableMap::getCurrentUser() const {
    const char* user = std::getenv("USER");
    if (!user) {
        user = std::getenv("USERNAME"); // Windows
    }
    return user ? std::string(user) : "unknown";
}

std::string templateSourceTypeToString(TemplateSourceType type) {
    switch (type) {
        case TemplateSourceType::Official: return "official";
        case TemplateSourceType::Git: return "git";
        case TemplateSourceType::Local: return "local";
    }
    return "unknown";
}

TemplateSourceType stringToTemplateSourceType(const std::string& str) {
    if (str == "official") return TemplateSourceType::Official;
    if (str == "git") return TemplateSourceType::Git;
    if (str == "local") return TemplateSourceType::Local;

    throw std::invalid_argument("Invalid template source type: " + str);
}

} // namespace scrap::template_system::model
