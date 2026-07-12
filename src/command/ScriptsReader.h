#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace scrap::Command {

/**
 * @brief Definition of a single project script from scrap.toml [scripts].
 */
struct ScriptDef {
    std::string name;
    std::string description;
    std::string command;
};

/**
 * @brief Abstract interface for reading project script definitions.
 *
 * Concrete implementations (provided by the configuration module)
 * parse scrap.toml's [scripts] section.  The stub implementation
 * always returns an empty list.
 */
class ScriptsReader {
public:
    virtual ~ScriptsReader();
    ScriptsReader(const ScriptsReader&) = default;
    ScriptsReader& operator=(const ScriptsReader&) = default;
    ScriptsReader(ScriptsReader&&) = default;
    ScriptsReader& operator=(ScriptsReader&&) = default;

    /**
     * @brief Read script definitions from the project configuration.
     *
     * @param projectRoot Path to the project root containing scrap.toml.
     * @return Script definitions on success, or an error message on failure.
     */
    [[nodiscard]] virtual auto
    read(const std::filesystem::path& projectRoot) -> std::expected<std::vector<ScriptDef>, std::string> = 0;

protected:
    ScriptsReader() = default;
};

}  // namespace scrap::Command
