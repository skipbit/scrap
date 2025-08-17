# scrap 🔧

<div align="center">

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)]()
[![Version](https://img.shields.io/badge/version-0.0.1--alpha-orange.svg)]()

**A Modern C++ Package Manager and Build System**

[Quick Start](#-quick-start) • [Features](#-features) • [Installation](#-installation) • [Documentation](#-documentation) • [Contributing](#-contributing)

</div>

---

## 🎯 What is scrap?

scrap is a modern development tool for C++ that brings the simplicity and power of Rust's Cargo to the C++ ecosystem. It provides a unified interface for project management, dependency handling, and build orchestration - all without the traditional complexity of C++ development.

### Why scrap?

C++ development has traditionally suffered from:

- **🔧 Complex build systems** - CMake, Make, Bazel, Meson... each with its own learning curve
- **📦 No standard package manager** - Unlike Rust, Go, or Node.js, C++ lacks a unified ecosystem
- **🖥️ System-dependent toolchains** - "It works on my machine" is too common
- **📚 High barrier to entry** - Setting up a C++ project shouldn't require a PhD

scrap solves these problems by providing:

- **🚀 Zero-configuration project setup** - Get started in seconds, not hours
- **📁 Standardized project structure** - Convention over configuration
- **🔄 Reproducible builds** - Same toolchain, same results, everywhere
- **📦 Modern package management** - GitHub-based ecosystem for sharing libraries
- **🛠️ Integrated toolchain management** - No more system dependency hell

## 🚀 Quick Start

```bash
# Create a new C++ application
scrap new my-app

# Create a new C++ library
scrap new my-lib --type=lib

# Build your project
cd my-app
scrap build

# Run your application
scrap run

# Clean build artifacts
scrap clean
```

That's it! No CMakeLists.txt, no Makefiles, no complex setup. scrap handles everything.

## ✨ Features

### 📝 Simple Configuration

Projects are configured with a simple `scrap.toml` file:

```toml
[package]
name = "my-app"
version = "0.1.0"
type = "app"
std = "23"

[[bin]]
name = "my-app"
src = "src/main.cpp"

[dependencies]
fmt = "10.2.1"
boost = { version = "1.84.0", features = ["filesystem", "asio"] }
```

### 🎨 Project Templates

Get started quickly with built-in templates:

```bash
# Use the default app template
scrap new my-project

# Use a specific template
scrap new my-game --template=game-engine

# Use a GitHub template
scrap new my-tool --template=github:user/template-repo
```

### 🔧 Integrated Toolchain Management (Coming Soon)

```bash
# Install a specific toolchain
scrap toolchain install llvm@18.0.0

# List available toolchains
scrap toolchain list

# Select a toolchain for your project
scrap toolchain select gcc@13.2.0
```

### 📦 Modern Package Management (Planned)

```bash
# Add a dependency
scrap add fmt

# Search for packages
scrap search boost

# Update dependencies
scrap update
```

## 📋 Project Status

scrap is in early alpha development (v0.0.1). Currently implemented:

✅ **Core Features**
- Project creation (`scrap new`)
- Template system with variable substitution
- Basic command structure (build, run, clean)
- Configuration file parsing (`scrap.toml`)

🚧 **In Progress**
- Git-based template repository integration
- Build system implementation
- Toolchain management

📅 **Planned**
- Package registry and dependency management
- Cross-compilation support
- IDE integrations
- CI/CD templates

## 🛠️ Installation

### Prerequisites

- C++23 compatible compiler (GCC 13+, Clang 16+, MSVC 2022+)
- CMake 3.20 or higher
- Git

### Building from Source

```bash
# Clone the repository
git clone https://github.com/skipbit/scrap.git
cd scrap

# Create build directory
mkdir -p build/release
cd build/release

# Configure and build
cmake ../.. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel

# The executable will be at build/release/bin/scrap
```

### Install (Unix-like systems)

```bash
sudo cp build/release/bin/scrap /usr/local/bin/
```

## 📖 Documentation

Detailed documentation is being developed. For now:

- [Architecture Overview](docs/architecture/README.md)
- [Template System](docs/template-system.md)
- [Coding Style Guide](docs/CODINGSTYLE.md)

## 🌐 Ecosystem

scrap is building a comprehensive C++ ecosystem:

### Related Repositories

- [scrap-templates](https://github.com/skipbit/scrap-templates) - Official project templates
- [scrap-packages](https://github.com/skipbit/scrap-packages) (Coming Soon) - Package registry
- [scrap-toolchains](https://github.com/skipbit/scrap-toolchains) (Coming Soon) - Toolchain definitions

### Vision

Our goal is to create a Homebrew-like ecosystem for C++ development, where:
- Libraries are easily discoverable and installable
- Toolchains are managed independently from the system
- Projects are reproducible across different environments
- The community can easily contribute and share packages

## 🤝 Contributing

We welcome contributions! scrap is built with:

- **Clean Architecture** - Domain-driven design with clear separation of concerns
- **Modern C++23** - Leveraging the latest language features
- **SOLID Principles** - Extensible and maintainable codebase

### How to Contribute

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Follow our [coding style guide](docs/CODINGSTYLE.md)
4. Commit your changes
5. Push to your branch
6. Open a Pull Request

### Development Setup

```bash
# Clone with submodules
git clone --recursive https://github.com/skipbit/scrap.git

# Build in debug mode
cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
cmake --build build/debug --parallel

# Run tests
cmake --build build/debug --target test

# Or run tests directly
./build/debug/test/scrap_test
```

### Testing

The project uses Catch2 v3.7.1 for unit testing. Tests are automatically built when `BUILD_TESTS=ON`.

```bash
# Build and run all tests
cmake --build build/debug --target test

# Run tests with verbose output
ctest --test-dir build/debug --output-on-failure --verbose

# Run specific test executable
./build/debug/test/scrap_test

# Release build testing
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
cmake --build build/release --target test
```

**Test Structure:**
- `test/unit/` - Unit tests for individual components
- `test/helpers/` - Test utilities (TestPresenter, FileSystemHelper)
- `test/fixtures/` - Test data and mock templates

## 📊 Roadmap

### Phase 1: Foundation (Current)
- ✅ Command structure and template system
- 🚧 Configuration management
- 🚧 Basic build system

### Phase 2: Toolchain & Build
- ⏳ Toolchain management
- ⏳ Full build system implementation
- ⏳ Dependency resolution

### Phase 3: Package Ecosystem
- ⏳ Package registry
- ⏳ Binary package distribution
- ⏳ Package publishing tools

### Phase 4: Advanced Features
- ⏳ Cross-compilation
- ⏳ IDE integrations
- ⏳ CI/CD templates
- ⏳ SBOM generation

## 🎯 Design Philosophy

scrap follows these core principles:

1. **Convention over Configuration** - Sensible defaults that just work
2. **Reproducible Builds** - Same input, same output, everywhere
3. **Modern C++ First** - Built for C++17/20/23, not C++98
4. **Community Driven** - Open source, open development
5. **Developer Experience** - Making C++ development enjoyable

## 📜 License

scrap is MIT licensed. See [LICENSE](LICENSE) for details.

## 🙏 Acknowledgments

scrap is inspired by:
- [Cargo](https://doc.rust-lang.org/cargo/) - Rust's excellent package manager
- [Homebrew](https://brew.sh/) - The missing package manager for macOS
- [Poetry](https://python-poetry.org/) - Python's modern dependency management

Special thanks to all contributors and the C++ community for feedback and support.

## 📬 Contact

- **GitHub Issues**: [Report bugs or request features](https://github.com/skipbit/scrap/issues)
- **Discussions**: [Join the conversation](https://github.com/skipbit/scrap/discussions)

---

<div align="center">

**Made with ❤️ for the C++ Community**

[⬆ Back to top](#scrap-)

</div>
