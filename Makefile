# The scrap Development Makefile
# Common development tasks and quality checks

.PHONY: help build test clean format lint check-headers check-all

# Default target
help:
	@echo "The scrap Development Commands:"
	@echo ""
	@echo "Building:"
	@echo "  build          - Build debug version"
	@echo "  build-release  - Build release version"
	@echo "  clean          - Clean build artifacts"
	@echo ""
	@echo "Testing:"
	@echo "  test           - Run unit tests"
	@echo "  test-verbose   - Run tests with verbose output"
	@echo ""
	@echo "Code Quality:"
	@echo "  format         - Apply clang-format to all source files"
	@echo "  lint           - Run clang-tidy on all source files"
	@echo "  check-headers  - Check headers contain declarations only"
	@echo "  check-all      - Run all quality checks (format, lint, headers)"
	@echo ""
	@echo "Development:"
	@echo "  setup          - Set up development environment"
	@echo "  deps           - Install development dependencies"

# Build targets
build:
	cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
	cmake --build build/debug --parallel

build-release:
	cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
	cmake --build build/release --parallel

clean:
	rm -rf build/
	rm -rf .cache/

# Test targets
test: build
	ctest --test-dir build/debug --output-on-failure

test-verbose: build
	ctest --test-dir build/debug --output-on-failure --verbose

# Code quality targets
format:
	@echo "🎨 Applying clang-format..."
	find src test -name "*.h" -o -name "*.cpp" | xargs clang-format -i
	@echo "✅ Formatting complete"

lint:
	@echo "🔍 Running clang-tidy..."
	find src -name "*.h" -o -name "*.cpp" | xargs -I {} /Library/Homebrew/opt/llvm/bin/clang-tidy {} -- -I./src -I./build/debug/include -std=c++23 -x c++
	@echo "✅ Linting complete"

check-headers:
	@echo "📋 Checking header declaration compliance..."
	./scripts/check-header-declarations.sh

check-all: format lint check-headers
	@echo "🎯 All quality checks complete"

# Development setup
setup:
	@echo "🛠️  Setting up development environment..."
	# Ensure required tools are available
	which cmake || (echo "❌ CMake not found. Please install CMake." && exit 1)
	which clang-format || (echo "❌ clang-format not found. Please install LLVM." && exit 1)
	which /Library/Homebrew/opt/llvm/bin/clang-tidy || (echo "❌ clang-tidy not found. Please install LLVM via Homebrew." && exit 1)
	chmod +x scripts/*.sh
	@echo "✅ Development environment ready"

deps:
	@echo "📦 Installing development dependencies..."
	# This would install system dependencies if needed
	# For now, just ensure our scripts are executable
	chmod +x scripts/*.sh
	@echo "✅ Dependencies ready"

# Build system integration
cmake-check-headers:
	$(MAKE) check-headers
