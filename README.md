# scrap 🔧

<div align="center">

[![Build](https://github.com/skipbit/scrap/actions/workflows/main-checks.yml/badge.svg?branch=main)](https://github.com/skipbit/scrap/actions/workflows/main-checks.yml)
[![codecov](https://codecov.io/gh/skipbit/scrap/branch/main/graph/badge.svg)](https://codecov.io/gh/skipbit/scrap)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)

**A Modern C++ Package Manager and Build System**

[Quick Start](#-quick-start) • [Features](#-features) • [Project Status](#-project-status) • [Installation](#-installation) • [Contributing](#-contributing)

</div>

---

## 🎯 What is scrap?

scrap is a modern development tool for C++ that brings the simplicity and power of Rust's Cargo to the C++ ecosystem. It aims to provide a unified interface for project management, dependency handling, and build orchestration - all without the traditional complexity of C++ development.

### Why scrap?

C++ development has traditionally suffered from:

- **🔧 Complex build systems** - CMake, Make, Bazel, Meson... each with its own learning curve
- **📦 No standard package manager** - Unlike Rust, Go, or Node.js, C++ lacks a unified ecosystem
- **🖥️ System-dependent toolchains** - "It works on my machine" is too common
- **📚 High barrier to entry** - Setting up a C++ project shouldn't require a PhD

scrap aims to solve these problems with:

- **🚀 Zero-configuration project setup** - Get started in seconds, not hours
- **📁 Standardized project structure** - Convention over configuration
- **🔄 Reproducible builds** - Same toolchain, same results, everywhere
- **📦 Modern package management** - GitHub-based ecosystem for sharing libraries
- **🛠️ Integrated toolchain management** - No more system dependency hell

What works today is listed under [Project Status](#-project-status).

## 🚀 Quick Start

```bash
# Create a new C++ application
scrap new my-app

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
std = "23"

[[bin]]
name = "my-app"
src = "src/main.cpp"
```

`std` names the C++ standard to build against: `"11"`, `"14"`, `"17"`, `"20"`, `"23"` (the default) or `"26"`. Whether a given compiler can build the one you name is the compiler's to answer, and `scrap build` says so before it compiles anything.

A target can add to the commands that build it. Settings written on a target apply to it alone; a library's settings under `[lib.public]` also apply to every executable that uses it:

```toml
[[lib]]
name = "core"
include-dirs = ["src"]
compile-flags = ["-Werror"]

[lib.public]
compile-flags = ["-pthread"]
link-flags = ["-pthread"]
```

### 🎨 Project Templates

Get started quickly with the built-in template:

```bash
# Create a project from the default app template
scrap new my-project
```

Choosing another template with `--template` is coming soon.

### 🔧 Integrated Toolchain Management

```bash
# Install the newest version of a series, which becomes the default if none is set
scrap toolchain install llvm@18

# Select an installed toolchain as the global default
scrap toolchain select llvm@18

# Remove the global default
scrap toolchain unselect
```

A project can name its own toolchain in `scrap.toml`, which comes before the global default; `system` names the compiler of the system, found through `CXX` or on `PATH`:

```toml
[toolchain]
name = "llvm@18"
```

The `SCRAP_TOOLCHAIN` environment variable comes before both. A toolchain that is named and not installed is an error; scrap does not install it in passing.

A program built with an installed toolchain runs with the runtime libraries of that toolchain; with LLVM, that is the libc++ it carries. On Linux, the linker of LLVM 17 to 22 needs `libxml2` on the machine. On macOS, an installed LLVM uses the SDK that `xcrun` names, which the Command Line Tools provide, or the one `SDKROOT` names. An LLVM installed by an earlier version of scrap has to be installed again with `--force`, as a build with it says.

## 📋 Project Status

scrap is in early alpha development.

✅ **Works today**
- **Creating a project** - `scrap new <name>` creates a project from the built-in template
- **Building** - `scrap build` makes a debug build, or an optimised one with `--release`, below `build/`, along with a `compile_commands.json` for editors and other tools. `--target <name>` builds one target
- **Building only what changed** - a source is compiled again when it, a header it includes, the compiler or the command that compiles it has changed
- **Static libraries** - with a `[[lib]]` in `scrap.toml`, the sources below `src/` become a static library that each executable links
- **Settings per target** - include directories, defines and flags, as shown in [Simple Configuration](#-simple-configuration)
- **Running** - `scrap run` builds the executable and starts it with the arguments after `--`, and exits with its exit code. `--bin <name>` chooses the executable when there is more than one
- **Cleaning** - `scrap clean` removes `build/`
- **Toolchains** - `scrap toolchain install`, `select` and `unselect`, and a toolchain per project in `scrap.toml`, as shown in [Integrated Toolchain Management](#-integrated-toolchain-management)
- **External commands** - an executable named `scrap-<name>` on `PATH` runs as `scrap <name>` and is listed in `scrap --help`

🚧 **In Progress**
- Choosing a template with `--template`, and templates from Git repositories
- `scrap template list` and `scrap template update`
- `scrap toolchain list`
- Choosing the number of parallel jobs
- `[dependencies]` in `scrap.toml`, which is accepted and not yet read

📅 **Planned**
- Package management: dependency resolution, a package registry, and `scrap add`, `scrap search` and `scrap update`
- Binary package distribution and package publishing tools
- Cross-compilation
- IDE integrations
- CI/CD templates
- SBOM generation

## 🛠️ Installation

### Install a prebuilt binary

```bash
curl -fsSL https://skipbit.github.io/scrap/install.sh | sh
```

This installs the latest release to `~/.scrap/bin/scrap` (`$SCRAP_HOME/bin` when `SCRAP_HOME` is set), after checking the archive against its published sha256, and offers to add that directory to `PATH`. To install a specific release, give its version as the [releases page](https://github.com/skipbit/scrap/releases) names it, in place of `vX.Y.Z`:

```bash
curl -fsSL https://skipbit.github.io/scrap/install.sh | sh -s -- vX.Y.Z
```

Prebuilt binaries are available for:

- Linux x86_64, with glibc 2.29 or later
- macOS on Apple silicon, 15.0 or later

The archives, their checksums and build provenance attestations are on the [releases page](https://github.com/skipbit/scrap/releases).

### Building from Source

To build scrap yourself, you need:

- A C++23 compiler and standard library. CI builds and tests scrap with:
  - On Linux, GCC 13 and 15 with their libstdc++, and Clang 20 and 22 with libstdc++ 13, 14 and 15 and with the libc++ of the same version
  - On macOS, the Apple Clang of macOS 15 and 26
- A C compiler as well: the build fetches liblzma and links it in, and on Linux libcurl and Mbed TLS too. macOS uses the libcurl it ships with.
- CMake 3.25 or higher
- Git

Then build it and copy it to a directory on your `PATH`:

```bash
git clone https://github.com/skipbit/scrap.git
cd scrap

# Build scrap alone, without its tests
cmake --preset release-no-tests
cmake --build --preset release-no-tests --parallel

# Copy it to where the install script puts it
mkdir -p ~/.scrap/bin
cp build/release-no-tests/bin/scrap ~/.scrap/bin/
```

## 🌐 Ecosystem

scrap is building a comprehensive C++ ecosystem:

### Related Repositories

- [scrap-templates](https://github.com/skipbit/scrap-templates) - Official project templates
- [scrap-toolchain](https://github.com/skipbit/scrap-toolchain) - The toolchains that `scrap toolchain install` installs
- scrap-packages (Coming Soon) - Package registry

### Vision

Our goal is to create a Homebrew-like ecosystem for C++ development, where:
- Libraries are easily discoverable and installable
- Toolchains are managed independently from the system
- Projects are reproducible across different environments
- The community can easily contribute and share packages

## 🤝 Contributing

We welcome contributions! scrap is written in C++23, and its design aims for:

- **Clean Architecture** - Keep the core free of I/O, platform and command-line details
- **Single responsibility** - Give each unit one job, so that a change stays in one place

### How to Contribute

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Follow our [coding style guide](docs/CODINGSTYLE.md)
4. Commit your changes
5. Push to your branch
6. Open a Pull Request

### Development Setup

```bash
git clone https://github.com/skipbit/scrap.git

# Configure, build and run the tests in debug mode
cmake --workflow --preset debug

# Run the tests again after a change
cmake --build --preset debug --parallel
ctest --preset debug
```

### Testing

The project uses GoogleTest for unit and end-to-end testing, run through ctest. The tests are built unless `BUILD_TESTS` is set to `OFF`.

```bash
# Build and run all tests
cmake --workflow --preset debug

# Run tests with verbose output
ctest --preset debug --verbose

# Run the tests of one module directly, or the end-to-end tests
./build/debug/test/scrap_gtest_<module>
./build/debug/test/scrap_e2e

# Release build testing
cmake --workflow --preset release
```

**Test Structure:**
- `test/unit/command/` - Unit tests for the command layer
- `test/unit/file/` - Unit tests for reading and writing whole files
- `test/unit/digest/` - Unit tests for SHA-256 digests
- `test/unit/tar/` - Unit tests for reading and extracting tar archives
- `test/unit/json/` - Unit tests for the JSON parser
- `test/unit/xz/` - Unit tests for reading xz-compressed files
- `test/unit/fetch/` - Unit tests for fetching a URL
- `test/unit/registry/` - Unit tests for fetching toolchains from a container registry
- `test/unit/install/` - Unit tests for installing a fetched toolchain
- `test/unit/project/` - Unit tests for project configuration (`scrap.toml`) and layout
- `test/unit/process/` - Unit tests for running other programs
- `test/unit/toolchain/` - Unit tests for toolchain detection
- `test/unit/compile/` - Unit tests for the compile and link commands, the compiler drivers and the compilation database
- `test/unit/build/` - Unit tests for running the steps of a build
- `test/support/` - Helpers shared by the tests of more than one module
- `test/e2e/` - End-to-end tests that spawn the built `scrap` binary as a subprocess
- `test/cmake/` - Tests of the CMake modules, run in script mode

## 🎯 Design Philosophy

scrap follows these core principles:

1. **Convention over Configuration** - Sensible defaults that just work
2. **Reproducible Builds** - Same input, same output, everywhere
3. **Modern C++ First** - New projects start on a modern standard; older ones are there when a project needs them
4. **Community Driven** - Open source, open development
5. **Developer Experience** - Making C++ development enjoyable

## 📜 License

scrap is MIT licensed. See [LICENSE](LICENSE) for details. A release archive also holds THIRD_PARTY_LICENSES, with the licenses of the libraries built into the binary.

## 🙏 Acknowledgments

scrap is inspired by:
- [Cargo](https://doc.rust-lang.org/cargo/) - Rust's excellent package manager
- [Homebrew](https://brew.sh/) - The missing package manager for macOS
- [Poetry](https://python-poetry.org/) - Python's modern dependency management

Special thanks to all contributors and the C++ community for feedback and support.

## 📬 Contact

- **GitHub Issues**: [Report bugs or request features](https://github.com/skipbit/scrap/issues)

---

<div align="center">

**Made with ❤️ for the C++ Community**

[⬆ Back to top](#scrap-)

</div>
