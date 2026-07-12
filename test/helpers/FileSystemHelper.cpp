#include "FileSystemHelper.h"
#include <fstream>
#include <random>
#include <sstream>
#include <stdexcept>

namespace scrap::test {

// TempDirectory implementation
FileSystemHelper::TempDirectory::TempDirectory(const std::string& prefix)
    : shouldCleanup_(true)
{
    // Generate unique directory name
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(100000, 999999);

    auto tempBase = std::filesystem::temp_directory_path();
    const std::string dirName = prefix + std::to_string(dis(gen));
    path_ = tempBase / dirName;

    std::filesystem::create_directories(path_);
}

FileSystemHelper::TempDirectory::~TempDirectory()
{
    if (shouldCleanup_ && ! path_.empty()) {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
        // Ignore errors during cleanup
    }
}

FileSystemHelper::TempDirectory::TempDirectory(TempDirectory&& other) noexcept
    : path_(std::move(other.path_)), shouldCleanup_(other.shouldCleanup_)
{
    other.shouldCleanup_ = false;
    other.path_.clear();
}

FileSystemHelper::TempDirectory& FileSystemHelper::TempDirectory::operator=(TempDirectory&& other) noexcept
{
    if (this != &other) {
        // Clean up current directory if needed
        if (shouldCleanup_ && ! path_.empty()) {
            std::error_code ec;
            std::filesystem::remove_all(path_, ec);
        }

        path_ = std::move(other.path_);
        shouldCleanup_ = other.shouldCleanup_;
        other.shouldCleanup_ = false;
        other.path_.clear();
    }
    return *this;
}

// FileSystemHelper static methods
FileSystemHelper::TempDirectory FileSystemHelper::createTempDirectory(const std::string& prefix)
{
    return TempDirectory(prefix);
}

void FileSystemHelper::createFile(const std::filesystem::path& path, const std::string& content)
{
    // Ensure parent directory exists
    auto parent = path.parent_path();
    if (! parent.empty()) {
        std::filesystem::create_directories(parent);
    }

    std::ofstream file(path);
    if (! file) {
        throw std::runtime_error("Failed to create file: " + path.string());
    }
    file << content;
}

void FileSystemHelper::createDirectories(const std::filesystem::path& path)
{
    std::filesystem::create_directories(path);
}

std::string FileSystemHelper::readFile(const std::filesystem::path& path)
{
    std::ifstream file(path);  // NOLINT(misc-const-correctness)
    if (! file) {
        throw std::runtime_error("Failed to read file: " + path.string());
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool FileSystemHelper::exists(const std::filesystem::path& path)
{
    return std::filesystem::exists(path);
}

void FileSystemHelper::remove(const std::filesystem::path& path)
{
    if (std::filesystem::exists(path)) {
        std::filesystem::remove_all(path);
    }
}

std::vector<std::filesystem::path> FileSystemHelper::listFiles(const std::filesystem::path& path, bool recursive)
{

    std::vector<std::filesystem::path> files;

    if (! std::filesystem::exists(path)) {
        return files;
    }

    if (recursive) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
            if (entry.is_regular_file()) {
                files.push_back(entry.path());
            }
        }
    } else {
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            if (entry.is_regular_file()) {
                files.push_back(entry.path());
            }
        }
    }

    return files;
}

void FileSystemHelper::copyDirectory(const std::filesystem::path& from, const std::filesystem::path& to)
{

    std::filesystem::copy(
        from, to, std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing);
}

}  // namespace scrap::test
