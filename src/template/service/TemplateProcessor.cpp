#include "TemplateProcessor.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace scrap::template_system::service {

// SimpleTemplateProcessor static members
const std::regex SimpleTemplateProcessor::VARIABLE_PATTERN(R"(\{\{([^}|]+)(\|([^}]+))?\}\})");
const std::regex SimpleTemplateProcessor::TRANSFORM_PATTERN(R"(\{\{([^}|]+)\|([^}]+)\}\})");

std::string TemplateProcessor::processContent(const std::string& content, const VariableMap& variables) {
    std::string result = content;

    // Process conditionals first
    result = processConditionals(result, variables);

    // Then process variable substitutions
    result = substituteVariables(result, variables);

    return result;
}

std::string TemplateProcessor::processFileName(const std::string& name, const VariableMap& variables) {
    return SimpleTemplateProcessor::processFileName(name, variables);
}

void TemplateProcessor::processTemplateDirectory(const std::filesystem::path& templatePath,
                                               const std::filesystem::path& targetPath,
                                               const VariableMap& variables,
                                               const std::vector<std::string>& ignorePatterns) {
    if (!std::filesystem::exists(templatePath)) {
        throw std::runtime_error("Template path does not exist: " + templatePath.string());
    }

    // Load .scrap-ignore file if it exists
    auto allIgnorePatterns = ignorePatterns;
    auto ignoreFilePatterns = loadIgnoreFile(templatePath);
    allIgnorePatterns.insert(allIgnorePatterns.end(), ignoreFilePatterns.begin(), ignoreFilePatterns.end());

    // Always ignore template metadata files
    allIgnorePatterns.push_back("template.toml");
    allIgnorePatterns.push_back(".scrap-ignore");

    // Create target directory
    std::filesystem::create_directories(targetPath);

    // Process all files and directories recursively
    std::error_code ec;
    for (auto& entry : std::filesystem::recursive_directory_iterator(templatePath, ec)) {
        if (ec) {
            std::cerr << "Warning: Error accessing " << entry.path() << ": " << ec.message() << std::endl;
            continue;
        }

        auto relativePath = std::filesystem::relative(entry.path(), templatePath);

        // Check if file should be ignored
        if (shouldIgnoreFile(relativePath, allIgnorePatterns)) {
            continue;
        }

        // Process filename/directory name
        std::string processedName = processFileName(relativePath.string(), variables);
        auto targetFilePath = targetPath / processedName;

        if (entry.is_directory()) {
            // Create directory
            std::filesystem::create_directories(targetFilePath);
        } else if (entry.is_regular_file()) {
            // Process and copy file
            copyTemplateFile(entry.path(), targetFilePath, variables);
        }
    }
}

std::string TemplateProcessor::substituteVariables(const std::string& content, const VariableMap& variables) {
    std::string result = content;
    std::regex variablePattern(R"(\{\{([^}|]+)(\|([^}]+))?\}\})");
    std::smatch match;

    while (std::regex_search(result, match, variablePattern)) {
        std::string variableName = trim(match[1].str());
        std::string transform = match[3].matched ? trim(match[3].str()) : "";

        auto value = variables.get(variableName);
        std::string replacement;

        if (value) {
            replacement = *value;
            if (!transform.empty()) {
                replacement = variables.applyTransform(replacement, transform);
            }
        } else {
            // Variable not found, leave placeholder or use empty string
            replacement = "";
            std::cerr << "Warning: Variable '" << variableName << "' not found" << std::endl;
        }

        result.replace(match.position(), match.length(), replacement);
    }

    return result;
}

std::string TemplateProcessor::processVariableExpression(const std::string& expression, const VariableMap& variables) {
    // Handle variable with optional transform
    auto pipePos = expression.find('|');
    if (pipePos != std::string::npos) {
        std::string variableName = trim(expression.substr(0, pipePos));
        std::string transform = trim(expression.substr(pipePos + 1));

        auto value = variables.get(variableName);
        if (value) {
            return variables.applyTransform(*value, transform);
        }
    } else {
        std::string variableName = trim(expression);
        auto value = variables.get(variableName);
        if (value) {
            return *value;
        }
    }

    return "";
}

std::string TemplateProcessor::processConditionals(const std::string& content, const VariableMap& variables) {
    std::string result = content;

    // Simple conditional processing: {{#if variable}} ... {{/if}}
    std::regex conditionalPattern(R"(\{\{#if\s+([^}]+)\}\}(.*?)\{\{/if\}\})");
    std::smatch match;

    while (std::regex_search(result, match, conditionalPattern)) {
        std::string condition = trim(match[1].str());
        std::string conditionalContent = match[2].str();

        std::string replacement;
        if (evaluateCondition(condition, variables)) {
            // Recursively process the content inside the conditional
            replacement = processConditionals(conditionalContent, variables);
        }

        result.replace(match.position(), match.length(), replacement);
    }

    return result;
}

bool TemplateProcessor::evaluateCondition(const std::string& condition, const VariableMap& variables) {
    std::string trimmedCondition = trim(condition);

    // Handle negation
    bool negate = false;
    if (trimmedCondition.starts_with("!")) {
        negate = true;
        trimmedCondition = trim(trimmedCondition.substr(1));
    }

    // Check if it's a simple variable existence check
    if (trimmedCondition.find(' ') == std::string::npos) {
        auto value = variables.get(trimmedCondition);
        bool exists = value.has_value() && !value->empty() && *value != "false" && *value != "0";
        return negate ? !exists : exists;
    }

    // Handle simple comparisons (variable == value)
    auto eqPos = trimmedCondition.find("==");
    if (eqPos != std::string::npos) {
        std::string varName = trim(trimmedCondition.substr(0, eqPos));
        std::string expectedValue = trim(trimmedCondition.substr(eqPos + 2));

        // Remove quotes from expected value
        if ((expectedValue.starts_with("\"") && expectedValue.ends_with("\"")) ||
            (expectedValue.starts_with("'") && expectedValue.ends_with("'"))) {
            expectedValue = expectedValue.substr(1, expectedValue.length() - 2);
        }

        auto actualValue = variables.get(varName);
        bool equal = actualValue && *actualValue == expectedValue;
        return negate ? !equal : equal;
    }

    // Handle simple comparisons (variable != value)
    auto neqPos = trimmedCondition.find("!=");
    if (neqPos != std::string::npos) {
        std::string varName = trim(trimmedCondition.substr(0, neqPos));
        std::string expectedValue = trim(trimmedCondition.substr(neqPos + 2));

        // Remove quotes from expected value
        if ((expectedValue.starts_with("\"") && expectedValue.ends_with("\"")) ||
            (expectedValue.starts_with("'") && expectedValue.ends_with("'"))) {
            expectedValue = expectedValue.substr(1, expectedValue.length() - 2);
        }

        auto actualValue = variables.get(varName);
        bool equal = actualValue && *actualValue == expectedValue;
        return negate ? equal : !equal;
    }

    // Default: treat as variable existence check
    auto value = variables.get(trimmedCondition);
    bool exists = value.has_value() && !value->empty() && *value != "false" && *value != "0";
    return negate ? !exists : exists;
}

void TemplateProcessor::copyTemplateFile(const std::filesystem::path& sourcePath,
                                       const std::filesystem::path& targetPath,
                                       const VariableMap& variables) {
    // Create target directory if needed
    auto targetDir = targetPath.parent_path();
    if (!targetDir.empty() && !std::filesystem::exists(targetDir)) {
        std::filesystem::create_directories(targetDir);
    }

    // Read source file
    std::ifstream source(sourcePath);
    if (!source) {
        throw std::runtime_error("Cannot read template file: " + sourcePath.string());
    }

    std::string content((std::istreambuf_iterator<char>(source)),
                       std::istreambuf_iterator<char>());

    // Process content
    std::string processedContent = processContent(content, variables);

    // Write target file
    std::ofstream target(targetPath);
    if (!target) {
        throw std::runtime_error("Cannot write target file: " + targetPath.string());
    }

    target << processedContent;
}

bool TemplateProcessor::shouldIgnoreFile(const std::filesystem::path& filePath,
                                       const std::vector<std::string>& ignorePatterns) {
    std::string filePathStr = filePath.string();
    std::string fileName = filePath.filename().string();

    for (const auto& pattern : ignorePatterns) {
        // Simple pattern matching (support * wildcards)
        std::string regexPattern = pattern;

        // Convert glob pattern to regex
        std::replace(regexPattern.begin(), regexPattern.end(), '*', '.');
        regexPattern = ".*" + regexPattern + ".*";

        try {
            std::regex patternRegex(regexPattern);
            if (std::regex_match(filePathStr, patternRegex) ||
                std::regex_match(fileName, patternRegex)) {
                return true;
            }
        } catch (const std::exception&) {
            // Invalid regex, try simple string match
            if (filePathStr.find(pattern) != std::string::npos ||
                fileName.find(pattern) != std::string::npos) {
                return true;
            }
        }
    }

    return false;
}

std::string TemplateProcessor::trim(const std::string& str) {
    auto start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) {
        return "";
    }
    auto end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
}

std::vector<std::string> TemplateProcessor::loadIgnoreFile(const std::filesystem::path& templatePath) {
    std::vector<std::string> patterns;
    auto ignoreFile = templatePath / ".scrap-ignore";

    if (!std::filesystem::exists(ignoreFile)) {
        return patterns;
    }

    std::ifstream file(ignoreFile);
    std::string line;

    while (std::getline(file, line)) {
        line = trim(line);
        if (!line.empty() && !line.starts_with("#")) {
            patterns.push_back(line);
        }
    }

    return patterns;
}

// SimpleTemplateProcessor implementation

std::string SimpleTemplateProcessor::process(const std::string& content, const VariableMap& variables) {
    std::string result = content;
    std::smatch match;

    while (std::regex_search(result, match, VARIABLE_PATTERN)) {
        std::string variableName = match[1].str();
        std::string transform = match[3].matched ? match[3].str() : "";

        // Trim whitespace
        variableName.erase(0, variableName.find_first_not_of(" \t"));
        variableName.erase(variableName.find_last_not_of(" \t") + 1);

        auto value = variables.get(variableName);
        std::string replacement;

        if (value) {
            replacement = *value;
            if (!transform.empty()) {
                transform.erase(0, transform.find_first_not_of(" \t"));
                transform.erase(transform.find_last_not_of(" \t") + 1);
                replacement = variables.applyTransform(replacement, transform);
            }
        }

        result.replace(match.position(), match.length(), replacement);
    }

    return result;
}

std::string SimpleTemplateProcessor::processFileName(const std::string& filename, const VariableMap& variables) {
    return process(filename, variables);
}

} // namespace scrap::template_system::service
