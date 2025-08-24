#include "TomlPlusPlusDriver.h"
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <optional>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <string>
#include <toml++/toml.h>

namespace scrap::Configuration::Driver {

namespace {

// Helper functions for parsing TOML sections
void parsePackageSection(const toml::table& config, Configuration::Model::ProjectConfiguration& result)
{
    const auto* package = config["package"].as_table();
    if (package == nullptr) {
        return;
    }

    if (const auto* name = package->get("name")) {
        result.name = std::string(name->value_or(""));
    }
    if (const auto* version = package->get("version")) {
        result.version = std::string(version->value_or("0.1.0"));
    }
    if (const auto* type = package->get("type")) {
        const auto typeStr = std::string(type->value_or("app"));
        result.type = Configuration::Model::parseProjectType(typeStr);
    }
    if (const auto* std = package->get("std")) {
        result.cppStandard = std::string(std->value_or("23"));
    }
    if (const auto* toolchain = package->get("toolchain")) {
        const auto toolchainStr = std::string(toolchain->value_or(""));
        if (!toolchainStr.empty()) {
            const auto parseResult = Configuration::Model::ToolchainReference::parse(toolchainStr);
            if (parseResult.has_value()) {
                result.toolchain = parseResult.value();
            }
        }
    }
}

void parseCxxFlagsArray(const toml::node* cxxFlags, Configuration::Model::ProjectConfiguration& result)
{
    const auto* flagsArray = cxxFlags->as_array();
    if (flagsArray == nullptr) {
        return;
    }

    for (const auto& flag : *flagsArray) {
        const auto flagStr = flag.value<std::string>();
        if (flagStr.has_value()) {
            result.cxxFlags.push_back(*flagStr);
        }
    }
}

void parseLinkFlagsArray(const toml::node* linkFlags, Configuration::Model::ProjectConfiguration& result)
{
    const auto* flagsArray = linkFlags->as_array();
    if (flagsArray == nullptr) {
        return;
    }

    for (const auto& flag : *flagsArray) {
        const auto flagStr = flag.value<std::string>();
        if (flagStr.has_value()) {
            result.linkFlags.push_back(*flagStr);
        }
    }
}

void parseDefinesArray(const toml::node* defines, Configuration::Model::ProjectConfiguration& result)
{
    const auto* definesArray = defines->as_array();
    if (definesArray == nullptr) {
        return;
    }

    std::string defineList;
    for (const auto& define : *definesArray) {
        const auto defineStr = define.value<std::string>();
        if (defineStr.has_value()) {
            if (!defineList.empty()) {
                defineList += ",";
            }
            defineList += *defineStr;
        }
    }
    if (!defineList.empty()) {
        result.buildOptions["defines"] = defineList;
    }
}

void parseBuildSection(const toml::table& config, Configuration::Model::ProjectConfiguration& result)
{
    const auto* build = config["build"].as_table();
    if (build == nullptr) {
        return;
    }

    if (const auto* system = build->get("system")) {
        const auto systemStr = std::string(system->value_or("native"));
        result.buildSystem = Configuration::Model::parseBuildSystem(systemStr);
    }

    if (const auto* cxxFlags = build->get("cxx_flags")) {
        parseCxxFlagsArray(cxxFlags, result);
    }

    if (const auto* linkFlags = build->get("link_flags")) {
        parseLinkFlagsArray(linkFlags, result);
    }

    if (const auto* defines = build->get("defines")) {
        parseDefinesArray(defines, result);
    }
}

void parseDependenciesSection(const toml::table& config, Configuration::Model::ProjectConfiguration& result)
{
    const auto* dependencies = config["dependencies"].as_table();
    if (dependencies == nullptr) {
        return;
    }

    for (const auto& [key, value] : *dependencies) {
        const auto versionStr = value.value<std::string>();
        if (versionStr.has_value()) {
            result.dependencies[std::string(key.str())] = *versionStr;
        }
    }
}

void parseTestSection(const toml::table& config, Configuration::Model::ProjectConfiguration& result)
{
    const auto* test = config["test"].as_table();
    if (test == nullptr) {
        return;
    }

    if (const auto* framework = test->get("framework")) {
        result.testFramework = std::string(framework->value_or(""));
    }
}

// Helper functions for serializing TOML sections
toml::table createPackageSection(const Configuration::Model::ProjectConfiguration& config)
{
    toml::table package;
    package.insert("name", config.name);
    package.insert("version", config.version);
    package.insert("type", Configuration::Model::toString(config.type));
    package.insert("std", config.cppStandard);

    if (config.toolchain.has_value()) {
        package.insert("toolchain", config.toolchain->toString());
    }

    return package;
}

void addCxxFlagsToTable(const std::vector<std::string>& cxxFlags, toml::table& build)
{
    if (!cxxFlags.empty()) {
        toml::array cxxFlagsArray;
        for (const auto& flag : cxxFlags) {
            cxxFlagsArray.push_back(flag);
        }
        build.insert("cxx_flags", std::move(cxxFlagsArray));
    }
}

void addLinkFlagsToTable(const std::vector<std::string>& linkFlags, toml::table& build)
{
    if (!linkFlags.empty()) {
        toml::array linkFlagsArray;
        for (const auto& flag : linkFlags) {
            linkFlagsArray.push_back(flag);
        }
        build.insert("link_flags", std::move(linkFlagsArray));
    }
}

void addDefinesToTable(const std::map<std::string, std::string>& buildOptions, toml::table& build)
{
    const auto definesIt = buildOptions.find("defines");
    if (definesIt != buildOptions.end() && !definesIt->second.empty()) {
        toml::array definesArray;
        std::stringstream ss(definesIt->second);
        std::string define;
        while (std::getline(ss, define, ',')) {
            if (!define.empty()) {
                definesArray.push_back(define);
            }
        }
        if (!definesArray.empty()) {
            build.insert("defines", std::move(definesArray));
        }
    }
}

toml::table createBuildSection(const Configuration::Model::ProjectConfiguration& config)
{
    toml::table build;
    build.insert("system", Configuration::Model::toString(config.buildSystem));

    addCxxFlagsToTable(config.cxxFlags, build);
    addLinkFlagsToTable(config.linkFlags, build);
    addDefinesToTable(config.buildOptions, build);

    return build;
}

toml::table createDependenciesSection(const Configuration::Model::ProjectConfiguration& config)
{
    toml::table dependencies;
    for (const auto& [name, version] : config.dependencies) {
        dependencies.insert(name, version);
    }
    return dependencies;
}

toml::table createTestSection(const Configuration::Model::ProjectConfiguration& config)
{
    toml::table test;
    if (!config.testFramework.empty()) {
        test.insert("framework", config.testFramework);
    }
    return test;
}

void flattenToml(const toml::node& node, const std::string& prefix, std::map<std::string, std::string>& result)
{
    // Use an iterative approach instead of recursion to avoid misc-no-recursion warning
    std::stack<std::pair<std::reference_wrapper<const toml::node>, std::string>> nodeStack;
    nodeStack.emplace(std::cref(node), prefix);

    while (!nodeStack.empty()) {
        const auto [currentNode, currentPrefix] = nodeStack.top();
        nodeStack.pop();

        if (const auto* table = currentNode.get().as_table()) {
            for (const auto& [key, value] : *table) {
                const std::string newKey =
                    currentPrefix.empty() ? std::string(key.str()) : currentPrefix + "." + std::string(key.str());
                nodeStack.emplace(std::cref(value), newKey);
            }
        } else if (const auto stringValue = currentNode.get().value<std::string>()) {
            result[currentPrefix] = *stringValue;
        } else if (const auto intValue = currentNode.get().value<std::int64_t>()) {
            result[currentPrefix] = std::to_string(*intValue);
        } else if (const auto doubleValue = currentNode.get().value<double>()) {
            result[currentPrefix] = std::to_string(*doubleValue);
        } else if (const auto boolValue = currentNode.get().value<bool>()) {
            result[currentPrefix] = (*boolValue) ? "true" : "false";
        } else {
            // Fallback for unknown types
            result[currentPrefix] = "[unknown type]";
        }
    }
}

}  // anonymous namespace

class TomlPlusPlusDriver::Impl {
public:
    static std::optional<Configuration::Model::ProjectConfiguration>
    loadProjectConfiguration(const std::filesystem::path& filePath)
    {

        if (!std::filesystem::exists(filePath)) {
            return {};
        }

        try {
            const auto config = toml::parse_file(filePath.string());
            return parseProjectConfiguration(config);
        } catch (const toml::parse_error& e) {
            throw std::runtime_error("TOML parse error: " + std::string(e.what()));
        }
    }

    static void saveProjectConfiguration(const std::filesystem::path& filePath,
                                         const Configuration::Model::ProjectConfiguration& config)
    {

        const auto tomlTable = serializeProjectConfiguration(config);

        std::ofstream file(filePath);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open file for writing: " + filePath.string());
        }

        file << tomlTable;

        if (!file.good()) {
            throw std::runtime_error("Error writing to file: " + filePath.string());
        }
    }

    static std::optional<std::map<std::string, std::string>> loadKeyValues(const std::filesystem::path& filePath)
    {

        if (!std::filesystem::exists(filePath)) {
            return {};
        }

        try {
            const auto config = toml::parse_file(filePath.string());
            std::map<std::string, std::string> result;

            // Flatten TOML structure to key-value pairs
            flattenToml(config, "", result);

            return result;
        } catch (const toml::parse_error& e) {
            throw std::runtime_error("TOML parse error: " + std::string(e.what()));
        }
    }

    static void saveKeyValues(const std::filesystem::path& filePath,
                              const std::map<std::string, std::string>& keyValues)
    {

        toml::table tomlTable;

        for (const auto& [key, value] : keyValues) {
            tomlTable.insert(key, value);
        }

        std::ofstream file(filePath);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open file for writing: " + filePath.string());
        }

        file << tomlTable;

        if (!file.good()) {
            throw std::runtime_error("Error writing to file: " + filePath.string());
        }
    }

    static bool exists(const std::filesystem::path& filePath)
    {
        return std::filesystem::exists(filePath) && std::filesystem::is_regular_file(filePath);
    }

    static std::string validateSyntax(const std::filesystem::path& filePath)
    {
        if (!std::filesystem::exists(filePath)) {
            return "File does not exist";
        }

        try {
            [[maybe_unused]] auto result = toml::parse_file(filePath.string());
            return "";  // Valid
        } catch (const toml::parse_error& e) {
            return {e.what()};
        }
    }

private:
    static Configuration::Model::ProjectConfiguration parseProjectConfiguration(const toml::table& config)
    {
        Configuration::Model::ProjectConfiguration result;

        parsePackageSection(config, result);
        parseBuildSection(config, result);
        parseDependenciesSection(config, result);
        parseTestSection(config, result);

        return result;
    }

    static toml::table serializeProjectConfiguration(const Configuration::Model::ProjectConfiguration& config)
    {
        toml::table result;

        result.insert("package", createPackageSection(config));
        result.insert("build", createBuildSection(config));

        if (!config.dependencies.empty()) {
            result.insert("dependencies", createDependenciesSection(config));
        }

        if (!config.testFramework.empty()) {
            result.insert("test", createTestSection(config));
        }

        return result;
    }
};

// TomlPlusPlusDriver implementation

TomlPlusPlusDriver::TomlPlusPlusDriver() = default;

TomlPlusPlusDriver::~TomlPlusPlusDriver() = default;

std::optional<Configuration::Model::ProjectConfiguration>
TomlPlusPlusDriver::loadProjectConfiguration(const std::filesystem::path& filePath)
{
    return Impl::loadProjectConfiguration(filePath);
}

void TomlPlusPlusDriver::saveProjectConfiguration(const std::filesystem::path& filePath,
                                                  const Configuration::Model::ProjectConfiguration& config)
{
    Impl::saveProjectConfiguration(filePath, config);
}

std::optional<std::map<std::string, std::string>>
TomlPlusPlusDriver::loadKeyValues(const std::filesystem::path& filePath)
{
    return Impl::loadKeyValues(filePath);
}

void TomlPlusPlusDriver::saveKeyValues(const std::filesystem::path& filePath,
                                       const std::map<std::string, std::string>& keyValues)
{
    Impl::saveKeyValues(filePath, keyValues);
}

bool TomlPlusPlusDriver::exists(const std::filesystem::path& filePath)
{
    return Impl::exists(filePath);
}

std::string TomlPlusPlusDriver::validateSyntax(const std::filesystem::path& filePath)
{
    return Impl::validateSyntax(filePath);
}

}  // namespace scrap::Configuration::Driver
