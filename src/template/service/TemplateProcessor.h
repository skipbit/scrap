#pragma once

#include "template/model/Template.h"
#include <filesystem>
#include <regex>
#include <string>

namespace scrap::template_system::service {

using namespace model;

/**
 * @brief Advanced template processing engine with mustache-like syntax
 *
 * Supports:
 * - Variable substitution: {{variable}}
 * - Transform pipes: {{variable|transform}}
 * - Conditional sections: {{#if condition}} ... {{/if}}
 * - File/directory name substitution
 */
class TemplateProcessor {
public:
    /**
     * @brief Process template content with variable substitution
     * @param content Template content to process
     * @param variables Variable map for substitution
     * @return Processed content
     */
    std::string processContent(const std::string& content, const VariableMap& variables);

    /**
     * @brief Process file/directory name with variable substitution
     * @param name File or directory name with placeholders
     * @param variables Variable map for substitution
     * @return Processed name
     */
    std::string processFileName(const std::string& name, const VariableMap& variables);

    /**
     * @brief Process entire template directory
     * @param templatePath Path to template directory
     * @param targetPath Target directory for output
     * @param variables Variable map for substitution
     * @param ignorePatterns File patterns to ignore
     */
    void processTemplateDirectory(const std::filesystem::path& templatePath,
                                  const std::filesystem::path& targetPath,
                                  const VariableMap& variables,
                                  const std::vector<std::string>& ignorePatterns = {});

private:
    // Variable substitution
    std::string substituteVariables(const std::string& content, const VariableMap& variables);
    std::string processVariableExpression(const std::string& expression, const VariableMap& variables);

    // Conditional processing
    std::string processConditionals(const std::string& content, const VariableMap& variables);
    bool evaluateCondition(const std::string& condition, const VariableMap& variables);

    // File operations
    void copyTemplateFile(const std::filesystem::path& sourcePath,
                          const std::filesystem::path& targetPath,
                          const VariableMap& variables);

    bool shouldIgnoreFile(const std::filesystem::path& filePath, const std::vector<std::string>& ignorePatterns);

    // Utility functions
    std::string trim(const std::string& str);
    std::vector<std::string> loadIgnoreFile(const std::filesystem::path& templatePath);
};

/**
 * @brief Simple template processor for basic variable substitution
 *
 * This is a simpler implementation that only handles {{variable}} substitution
 * without advanced features like conditionals or loops.
 */
class SimpleTemplateProcessor {
public:
    /**
     * @brief Process content with simple variable substitution
     * @param content Template content
     * @param variables Variable map
     * @return Processed content
     */
    static std::string process(const std::string& content, const VariableMap& variables);

    /**
     * @brief Process filename with variable substitution
     * @param filename Filename with placeholders
     * @param variables Variable map
     * @return Processed filename
     */
    static std::string processFileName(const std::string& filename, const VariableMap& variables);

private:
    static const std::regex VARIABLE_PATTERN;
    static const std::regex TRANSFORM_PATTERN;
};

}  // namespace scrap::template_system::service
