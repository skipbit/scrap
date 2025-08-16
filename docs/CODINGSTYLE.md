# SCRAP Coding Style Guide

This document establishes unified coding style standards for the SCRAP project.
Consistent code style improves code readability, maintainability, and team development efficiency.

## Core Principles

### Architectural Principles

The SCRAP project is designed based on the following principles:

- **Domain-Driven Design (DDD)**: Aggregate business logic in domain layers
- **Clean Architecture**: Control dependency directions and remain independent from external details
- **SOLID Principles**: Extensible and maintainable design

### Compilation Environment

- **C++ Standard**: C++23
- **Character Encoding**: UTF-8
- **Line Endings**: LF (Unix format)
- **Compilation Settings**: `-Wall -Wextra -Wpedantic -Werror`

## Naming Conventions

### Class, Struct, and Type Names

```cpp
class TemplateService {
    // ...
};

struct TemplateVariable {
    // ...
};

using ProjectPtr = std::unique_ptr<Project>;
```

- **Format**: `UpperCamelCase`
- **Principle**: Use nouns or noun phrases

### Function and Method Names

```cpp
class Template {
public:
    const std::string& name() const;  // getter: no 'get' prefix
    void setName(const std::string& name);

    std::expected<void, Error> loadFromFile(const std::filesystem::path& path) noexcept;
    bool isValid() const noexcept;
    void processTemplate() const;
};
```

- **Format**: `lowerCamelCase`
- **Getters**: Do not use `get` prefix
- **Boolean functions**: Prefixes like `is`, `has`, `can` are recommended (not enforced)

### Variable Names

```cpp
class TemplateService {
private:
    std::string name_;              // private member
    std::filesystem::path templatePath_;

public:
    void processTemplate() {
        const auto fileName = templatePath_.filename();  // local variable
        const std::string templateName = "default";     // const-qualified
        // ...
    }
};
```

- **Format**: `lowerCamelCase`
- **Private members**: Trailing underscore `name_`
- **Public members**: No underscore

### Namespaces

```cpp
namespace scrap {                    // root namespace: lowercase
namespace Template {                 // child namespace: CamelCase
namespace Model {

class TemplateVariable {
    // ...
};

} // Model
} // Template
} // scrap
```

- **Root namespace**: `scrap` (lowercase)
- **Child namespaces**: `CamelCase`
- **Indentation**: Do not indent namespace contents
- **Closing comments**: Add closing comments for long namespaces

### File Names

```cpp
// Template.h
#pragma once

// Template.cpp
#include "Template.h"
```

- **Format**: `UpperCamelCase`
- **Header files**: `.h` extension
- **Implementation files**: `.cpp` extension
- **Include guards**: Use `#pragma once`

## Include Guards

### Use #pragma once

```cpp
// Template.h
#pragma once

#include <string>
#include <vector>

class Template {
    // ...
};
```

- **Standard**: Always use `#pragma once` for include guards
- **Prohibition**: Do not use traditional `#ifndef`/`#define`/`#endif` guards
- **Placement**: Place `#pragma once` as the first line in header files

## Formatting Rules

### Indentation and Spacing

```cpp
namespace scrap {

class Template {
public:
    Template();
    ~Template();

private:
    std::string name_;
    int value_;
};

void function()
{
    if (condition) {
        doSomething();
    }

    for (const auto& item : items) {
        processItem(item);
    }
}

} // scrap
```

- **Indentation**: 4 spaces (no tabs)
- **Namespaces**: Do not indent contents
- **Access modifiers**: -4 space offset

### Brace Placement

```cpp
// K&R style (default)
class MyClass {
    // ...
};

if (condition) {
    doSomething();
} else {
    doOtherThing();
}

// Allman style (function definitions only)
void functionDefinition()
{
    // implementation
}

// Constructor with initializer list
MyClass::MyClass(const std::string& name, int value)
    : name_(name), value_(value)
{
    // constructor implementation
}
```

- **Default**: K&R style (opening brace on same line)
- **Function definitions**: Allman style (opening brace on new line)
- **Constructor initializers**: Colon on new line with indentation, initializers on same line when possible
- **Single statements**: Braces are optional

### Line and File Endings

```cpp
// Files must end with a newline (not a new empty line)
class Example {
    void method();
};
// newline here (EOF)
```

- **File endings**: Must end with a newline
- **Empty lines**: Do not indent
- **Trailing whitespace**: Remove
- **Line width**: 120 characters recommended

## Error Handling

### Using std::expected

```cpp
#include <expected>

class FileLoader {
public:
    std::expected<std::string, Error> loadFile(const std::filesystem::path& path) noexcept
    {
        if (!std::filesystem::exists(path)) {
            return std::unexpected(Error::FileNotFound);
        }

        // file loading process
        return content;
    }
};

// Usage example
auto result = loader.loadFile("template.toml");
if (!result) {
    // error handling
    handleError(result.error());
    return;
}

const auto content = result.value();
```

- **Principle**: Exceptions (`throw`) are prohibited
- **Error type**: Use `std::expected<T, Error>`
- **Return value checking**: Always check results

### Exception Prohibition

```cpp
// ❌ Prohibited
void badFunction() {
    throw std::runtime_error("Error occurred");
}

// ✅ Recommended
std::expected<void, Error> goodFunction() noexcept {
    if (errorCondition) {
        return std::unexpected(Error::InvalidOperation);
    }
    return {};
}
```

## Aggressive Use of const/constexpr

### Local Variables

```cpp
void processData() {
    const auto fileName = getFileName();           // const
    const std::string baseName = "template";      // const
    constexpr int maxRetries = 3;                 // constexpr

    // non-const only when modification is necessary
    int retryCount = 0;
}
```

### Member Functions

```cpp
class Template {
public:
    const std::string& name() const noexcept;     // const + noexcept
    bool isEmpty() const noexcept;

    void setName(const std::string& name) noexcept;

private:
    mutable std::string cachedData_;              // mutable when needed
};
```

- **Read-only**: Always use `const` or `constexpr`
- **Member functions**: Use `const` when not modifying state
- **Exception safety**: Use `noexcept` when not throwing exceptions

## Aggressive Use of PIMPL Pattern

### Header File

```cpp
// GitDriver.h
#pragma once

#include <memory>
#include <string>
#include <expected>

namespace scrap::Repository {

class GitDriver {
public:
    GitDriver();
    ~GitDriver();

    // Disable copy, enable move
    GitDriver(const GitDriver&) = delete;
    GitDriver& operator=(const GitDriver&) = delete;
    GitDriver(GitDriver&&) noexcept;
    GitDriver& operator=(GitDriver&&) noexcept;

    std::expected<void, Error> clone(const std::string& url, const std::filesystem::path& path) noexcept;

private:
    class Impl;                    // forward declaration
    std::unique_ptr<Impl> impl_;   // PIMPL
};

} // Repository
```

### Implementation File

```cpp
// GitDriver.cpp
#include "GitDriver.h"
#include <git2.h>  // does not leak to header

namespace scrap::Repository {

class GitDriver::Impl {
public:
    git_repository* repo = nullptr;

    std::expected<void, Error> clone(const std::string& url, const std::filesystem::path& path) noexcept
    {
        // implementation using libgit2
        // implementation details do not leak to header
    }
};

GitDriver::GitDriver()
    : impl_(std::make_unique<Impl>())
{
}
GitDriver::~GitDriver() = default;
GitDriver::GitDriver(GitDriver&&) noexcept = default;
GitDriver& GitDriver::operator=(GitDriver&&) noexcept = default;

} // Repository
```

- **Purpose**: Ensure ABI safety, prevent header information leakage
- **Application**: Classes using external libraries
- **Special member functions**: Define appropriately

## Macro Restrictions

### Prohibit Constant Definitions

```cpp
// ❌ Prohibited
#define MAX_SIZE 100
#define VERSION_STRING "1.0.0"

// ✅ Recommended
constexpr int MaxSize = 100;
constexpr const char* VersionString = "1.0.0";

namespace Config {
    constexpr int DefaultBufferSize = 4096;
}
```

### Allow Only Utility Macros

```cpp
// ✅ Allowed utility macro examples
#define SCRAP_STRINGIFY(x) #x
#define SCRAP_CONCATENATE(a, b) a##b

// Debug build only macros
#ifdef DEBUG
#define SCRAP_DEBUG_PRINT(x) std::cout << x << std::endl
#else
#define SCRAP_DEBUG_PRINT(x)
#endif
```

## Aggressive Function Qualification

### noexcept Specification

```cpp
class TemplateService {
public:
    // Functions that don't throw exceptions
    const std::string& name() const noexcept;
    bool isValid() const noexcept;
    void clear() noexcept;

    // May return errors but don't throw exceptions
    std::expected<Template, Error> loadTemplate(const std::string& name) noexcept;
};
```

### Using constexpr

```cpp
class MathUtils {
public:
    static constexpr int square(int x) noexcept {
        return x * x;
    }

    static constexpr bool isPowerOfTwo(int x) noexcept {
        return x > 0 && (x & (x - 1)) == 0;
    }
};

// Compile-time calculation
constexpr int result = MathUtils::square(5);  // 25 calculated at compile time
```

## Handling Unused Parameters

```cpp
// ❌ Commenting out is prohibited
void callback(int /*unused*/, const std::string& message) {
    processMessage(message);
}

// ✅ Use [[maybe_unused]]
void callback([[maybe_unused]] int errorCode, const std::string& message) {
    processMessage(message);
}

// ✅ Omit name (when parameter is clear)
void setName(const std::string&);  // declaration in header

void ClassName::setName(const std::string& name) {  // name in implementation
    name_ = name;
}
```

## Comments and Documentation

### Doxygen Style

```cpp
/**
 * @brief Template loading and processing service
 *
 * This class provides functionality to load templates from various sources,
 * process template variables, and generate project files.
 */
class TemplateService {
public:
    /**
     * @brief Load a template by name
     * @param name Template name or source/name format
     * @return Template if found, error otherwise
     * @throws None (uses std::expected)
     */
    std::expected<Template, Error> loadTemplate(const std::string& name) noexcept;

private:
    std::string defaultSource_;  ///< Default template source name
};
```

### Inline Comments

```cpp
void processTemplate() {
    // Phase 1: Parse template metadata
    const auto metadata = parseMetadata();

    // Phase 2: Resolve template variables
    auto variables = resolveVariables(metadata);

    // Phase 3: Generate output files
    generateFiles(variables);
}
```

## Type Safety and Modern C++

### Using Smart Pointers

```cpp
class ResourceManager {
private:
    std::unique_ptr<Resource> resource_;              // exclusive ownership
    std::shared_ptr<SharedResource> sharedResource_;  // shared ownership
    std::weak_ptr<Observer> observer_;                // weak reference

public:
    std::unique_ptr<Resource> createResource() {
        return std::make_unique<Resource>();
    }
};
```

### Utilizing Type Aliases

```cpp
namespace scrap::Template {
    using VariableMap = std::unordered_map<std::string, std::string>;
    using TemplatePtr = std::unique_ptr<Template>;
    using LoadResult = std::expected<Template, Error>;
}
```

### Using Range-based for

```cpp
void processTemplates(const std::vector<Template>& templates) {
    for (const auto& tmpl : templates) {
        processTemplate(tmpl);
    }

    // When index is needed
    for (std::size_t i = 0; const auto& tmpl : templates) {
        processTemplate(i, tmpl);
        ++i;
    }
}
```

## Performance Guidelines

### Move Semantics

```cpp
class Template {
public:
    void setName(std::string name) {  // pass by value
        name_ = std::move(name);      // move
    }

    std::vector<std::string> getTags() && {  // rvalue-only
        return std::move(tags_);
    }

private:
    std::string name_;
    std::vector<std::string> tags_;
};
```

### Avoiding Unnecessary Copies

```cpp
// ✅ Recommended
void processLargeData(const std::vector<LargeObject>& data) {
    for (const auto& item : data) {  // receive by reference
        process(item);
    }
}

// ❌ Avoid
void processLargeData(std::vector<LargeObject> data) {  // unnecessary copy
    for (auto item : data) {  // further unnecessary copy
        process(item);
    }
}
```

## Summary

This coding style guide was established to improve the quality and maintainability of the SCRAP project.
By having all team members follow these rules, we can maintain a consistent and readable codebase.

If you have any questions or suggestions regarding the style, please contact the project maintainers.
