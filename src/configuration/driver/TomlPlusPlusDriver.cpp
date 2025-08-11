#include "TomlPlusPlusDriver.h"
#include <toml++/toml.h>
#include <fstream>
#include <stdexcept>

namespace scrap::configuration::driver {

class TomlPlusPlusDriver::Impl {
public:
    std::optional<model::ProjectConfiguration> loadProjectConfiguration(
        const std::filesystem::path& filePath) {

        if (!std::filesystem::exists(filePath)) {
            return std::nullopt;
        }

        try {
            auto config = toml::parse_file(filePath.string());
            return parseProjectConfiguration(config);
        } catch (const toml::parse_error& e) {
            throw std::runtime_error("TOML parse error: " + std::string(e.what()));
        }
    }

    void saveProjectConfiguration(
        const std::filesystem::path& filePath,
        const model::ProjectConfiguration& config) {

        auto tomlTable = serializeProjectConfiguration(config);

        std::ofstream file(filePath);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open file for writing: " + filePath.string());
        }

        file << tomlTable;

        if (!file.good()) {
            throw std::runtime_error("Error writing to file: " + filePath.string());
        }
    }

    std::optional<std::map<std::string, std::string>> loadKeyValues(
        const std::filesystem::path& filePath) {

        if (!std::filesystem::exists(filePath)) {
            return std::nullopt;
        }

        try {
            auto config = toml::parse_file(filePath.string());
            std::map<std::string, std::string> result;

            // Flatten TOML structure to key-value pairs
            flattenToml(config, "", result);

            return result;
        } catch (const toml::parse_error& e) {
            throw std::runtime_error("TOML parse error: " + std::string(e.what()));
        }
    }

    void saveKeyValues(
        const std::filesystem::path& filePath,
        const std::map<std::string, std::string>& keyValues) {

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

    bool exists(const std::filesystem::path& filePath) {
        return std::filesystem::exists(filePath) && std::filesystem::is_regular_file(filePath);
    }

    std::string validateSyntax(const std::filesystem::path& filePath) {
        if (!std::filesystem::exists(filePath)) {
            return "File does not exist";
        }

        try {
            [[maybe_unused]] auto result = toml::parse_file(filePath.string());
            return ""; // Valid
        } catch (const toml::parse_error& e) {
            return std::string(e.what());
        }
    }

private:
    model::ProjectConfiguration parseProjectConfiguration(const toml::table& config) {
        model::ProjectConfiguration result;

        // Parse [project] section
        if (auto project = config["project"].as_table()) {
            if (auto name = project->get("name")) {
                result.name = std::string(name->value_or(""));
            }
            if (auto version = project->get("version")) {
                result.version = std::string(version->value_or("0.1.0"));
            }
            if (auto type = project->get("type")) {
                auto typeStr = std::string(type->value_or("app"));
                result.type = model::parseProjectType(typeStr);
            }
            if (auto std = project->get("std")) {
                result.cppStandard = std::string(std->value_or("23"));
            }
        }

        // Parse [build] section
        if (auto build = config["build"].as_table()) {
            if (auto system = build->get("system")) {
                auto systemStr = std::string(system->value_or("native"));
                result.buildSystem = model::parseBuildSystem(systemStr);
            }
            if (auto toolchain = build->get("toolchain")) {
                auto toolchainStr = std::string(toolchain->value_or(""));
                if (!toolchainStr.empty()) {
                    result.toolchain = model::ToolchainReference::parse(toolchainStr);
                }
            }
        }

        // Parse [dependencies] section
        if (auto deps = config["dependencies"].as_table()) {
            for (auto&& [key, value] : *deps) {
                if (auto strValue = value.value<std::string>()) {
                    result.dependencies[std::string(key.str())] = *strValue;
                }
            }
        }

        // Parse [dev-dependencies] section
        if (auto devDeps = config["dev-dependencies"].as_table()) {
            for (auto&& [key, value] : *devDeps) {
                if (auto strValue = value.value<std::string>()) {
                    result.devDependencies[std::string(key.str())] = *strValue;
                }
            }
        }

        // Parse [test] section
        if (auto test = config["test"].as_table()) {
            if (auto framework = test->get("framework")) {
                result.testFramework = std::string(framework->value_or("scrap"));
            }
        }

        result.validate();
        return result;
    }

    toml::table serializeProjectConfiguration(const model::ProjectConfiguration& config) {
        toml::table result;

        // [project] section
        toml::table project;
        project.insert("name", config.name);
        project.insert("version", config.version);
        project.insert("type", model::toString(config.type));
        project.insert("std", config.cppStandard);
        result.insert("project", project);

        // [build] section
        toml::table build;
        build.insert("system", model::toString(config.buildSystem));
        if (config.toolchain) {
            build.insert("toolchain", config.toolchain->toString());
        }
        result.insert("build", build);

        // [dependencies] section
        if (!config.dependencies.empty()) {
            toml::table deps;
            for (const auto& [name, version] : config.dependencies) {
                deps.insert(name, version);
            }
            result.insert("dependencies", deps);
        }

        // [dev-dependencies] section
        if (!config.devDependencies.empty()) {
            toml::table devDeps;
            for (const auto& [name, version] : config.devDependencies) {
                devDeps.insert(name, version);
            }
            result.insert("dev-dependencies", devDeps);
        }

        // [test] section
        toml::table test;
        test.insert("framework", config.testFramework);
        result.insert("test", test);

        return result;
    }

    void flattenToml(const toml::node& node, const std::string& prefix,
                     std::map<std::string, std::string>& result) {
        if (auto table = node.as_table()) {
            for (auto&& [key, value] : *table) {
                std::string newKey = prefix.empty() ? std::string(key.str()) : prefix + "." + std::string(key.str());
                flattenToml(value, newKey, result);
            }
        } else if (auto value = node.value<std::string>()) {
            result[prefix] = *value;
        } else if (auto intValue = node.value<int64_t>()) {
            result[prefix] = std::to_string(*intValue);
        } else if (auto doubleValue = node.value<double>()) {
            result[prefix] = std::to_string(*doubleValue);
        } else if (auto boolValue = node.value<bool>()) {
            result[prefix] = *boolValue ? "true" : "false";
        } else {
            // Fallback for unknown types
            result[prefix] = "[unknown type]";
        }
    }
};

// TomlPlusPlusDriver implementation

TomlPlusPlusDriver::TomlPlusPlusDriver() : impl_(std::make_unique<Impl>()) {}

TomlPlusPlusDriver::~TomlPlusPlusDriver() = default;

std::optional<model::ProjectConfiguration> TomlPlusPlusDriver::loadProjectConfiguration(
    const std::filesystem::path& filePath) {
    return impl_->loadProjectConfiguration(filePath);
}

void TomlPlusPlusDriver::saveProjectConfiguration(
    const std::filesystem::path& filePath,
    const model::ProjectConfiguration& config) {
    impl_->saveProjectConfiguration(filePath, config);
}

std::optional<std::map<std::string, std::string>> TomlPlusPlusDriver::loadKeyValues(
    const std::filesystem::path& filePath) {
    return impl_->loadKeyValues(filePath);
}

void TomlPlusPlusDriver::saveKeyValues(
    const std::filesystem::path& filePath,
    const std::map<std::string, std::string>& keyValues) {
    impl_->saveKeyValues(filePath, keyValues);
}

bool TomlPlusPlusDriver::exists(const std::filesystem::path& filePath) {
    return impl_->exists(filePath);
}

std::string TomlPlusPlusDriver::validateSyntax(const std::filesystem::path& filePath) {
    return impl_->validateSyntax(filePath);
}

} // namespace scrap::configuration::driver
