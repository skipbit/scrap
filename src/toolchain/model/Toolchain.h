#pragma once

#include <string>
#include <vector>

namespace scrap::toolchain {

/**
 * @brief Toolchain domain entity representing a compiler toolchain
 * 
 * This class represents a toolchain in the domain model following DDD principles.
 */
class Toolchain {
public:
    Toolchain(const std::string& name, const std::string& version, const std::string& architecture);
    ~Toolchain() = default;
    
    // Copy and move operations
    Toolchain(const Toolchain&) = default;
    Toolchain& operator=(const Toolchain&) = default;
    Toolchain(Toolchain&&) = default;
    Toolchain& operator=(Toolchain&&) = default;
    
    /**
     * @brief Get the toolchain name (e.g., "gcc", "llvm")
     * @return Toolchain name
     */
    const std::string& getName() const;
    
    /**
     * @brief Get the toolchain version (e.g., "13.2.0", "18.0.0")
     * @return Toolchain version
     */
    const std::string& getVersion() const;
    
    /**
     * @brief Get the target architecture (e.g., "x86_64", "aarch64")
     * @return Target architecture
     */
    const std::string& getArchitecture() const;
    
    /**
     * @brief Get the full toolchain identifier
     * @return Full identifier in format "name-version-architecture"
     */
    std::string getFullIdentifier() const;
    
    /**
     * @brief Check if this toolchain is currently selected as default
     * @return True if this is the default toolchain
     */
    bool isDefault() const;
    
    /**
     * @brief Mark this toolchain as default or not
     * @param isDefault Whether this toolchain should be default
     */
    void setDefault(bool isDefault);

private:
    std::string name_;
    std::string version_;
    std::string architecture_;
    bool isDefault_;
};

}