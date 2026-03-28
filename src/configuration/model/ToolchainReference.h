#pragma once

#include <cstdint>
#include <dross/type/error.h>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <system_error>

namespace scrap::Configuration::Model {

/**
 * @brief Error codes for ToolchainReference operations
 */
enum class ToolchainReferenceError : std::uint8_t {
    EmptyName,              ///< Toolchain name cannot be empty
    EmptySpecification,     ///< Toolchain specification cannot be empty
    EmptyVersion,           ///< Version cannot be empty after '@'
    SystemDefaultHasNoName  ///< System default toolchain has no name
};

}  // namespace scrap::Configuration::Model

// Error code creation function declaration (must be in global scope for ADL)
// NOLINTNEXTLINE(readability-identifier-naming) - C++ standard requires this exact name for ADL
std::error_code make_error_code(scrap::Configuration::Model::ToolchainReferenceError e) noexcept;

// C++ standard requires specializing std::is_error_code_enum for custom error enums
namespace std {
template <> struct is_error_code_enum<scrap::Configuration::Model::ToolchainReferenceError> : true_type { };
}  // namespace std

namespace scrap::Configuration::Model {

/**
 * @brief Value Object representing a toolchain reference
 *
 * Can represent either:
 * - A managed toolchain (name + optional version)
 * - System default toolchain (not managed by scrap)
 */
class ToolchainReference {
public:
    /**
     * @brief Default constructor creates system default toolchain reference
     */
    ToolchainReference();

    /**
     * @brief Copy constructor
     */
    ToolchainReference(const ToolchainReference& other);

    /**
     * @brief Copy assignment operator
     */
    ToolchainReference& operator=(const ToolchainReference& other);

    /**
     * @brief Move constructor
     */
    ToolchainReference(ToolchainReference&& other) noexcept;

    /**
     * @brief Move assignment operator
     */
    ToolchainReference& operator=(ToolchainReference&& other) noexcept;

    /**
     * @brief Destructor
     */
    ~ToolchainReference();

    /**
     * @brief Create system default toolchain reference
     */
    [[nodiscard]] static ToolchainReference createSystemDefault();

    /**
     * @brief Create managed toolchain reference
     * @param name Toolchain name (e.g., "llvm", "gcc")
     * @param version Optional version (e.g., "18.0.0")
     */
    [[nodiscard]] static std::expected<ToolchainReference, dross::error>
    createManaged(const std::string& name, const std::optional<std::string>& version = std::nullopt) noexcept;

    /**
     * @brief Parse toolchain reference from string
     * @param spec Specification string (e.g., "llvm@18.0.0", "gcc", "system")
     */
    [[nodiscard]] static std::expected<ToolchainReference, dross::error> parse(const std::string& spec) noexcept;

    // Value Object - equality comparison
    [[nodiscard]] bool operator==(const ToolchainReference& other) const;
    [[nodiscard]] bool operator!=(const ToolchainReference& other) const;

    /**
     * @brief Check if this represents system default toolchain
     */
    [[nodiscard]] bool isSystemDefault() const noexcept;

    /**
     * @brief Get toolchain name
     * @return Toolchain name or error if system default
     */
    [[nodiscard]] std::expected<std::string, dross::error> name() const noexcept;

    /**
     * @brief Get toolchain version (nullopt if no version specified or system default)
     */
    [[nodiscard]] const std::optional<std::string>& version() const noexcept;

    /**
     * @brief Convert to string representation
     */
    [[nodiscard]] std::string toString() const;

private:
    // PIMPL forward declaration
    class Internal;
    std::unique_ptr<Internal> impl_;
};

}  // namespace scrap::Configuration::Model
