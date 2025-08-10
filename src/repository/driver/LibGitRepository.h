#pragma once

#include <filesystem>
#include <memory>

namespace scrap::repository::libgit {

class Repository {
public:
    Repository(const std::filesystem::path& path);
    Repository(const std::string& url, const std::filesystem::path& path);
    ~Repository();

    void update(const std::string& remote, const std::string& branch);

private:
    class Internal;
    std::unique_ptr<Internal> impl_;
};

}
