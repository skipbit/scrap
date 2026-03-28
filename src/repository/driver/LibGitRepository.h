#pragma once

#include <expected>
#include <filesystem>
#include <memory>
#include <system_error>

namespace scrap::repository::libgit {

class Repository {
public:
    Repository(const std::filesystem::path& path);
    Repository(const std::string& url, const std::filesystem::path& path);
    ~Repository();

    std::expected<void, std::error_code> update(const std::string& remote, const std::string& branch);

private:
    class Internal;
    std::unique_ptr<Internal> impl_;
};

}  // namespace scrap::repository::libgit
