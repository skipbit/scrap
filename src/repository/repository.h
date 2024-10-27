#pragma once

#include <string>
#include <filesystem>
#include <memory>

namespace scrap {

class repository {
public:
    enum class type {
        git,
    };

    class driver {
    public:
        virtual ~driver() = default;
        virtual void clone(const std::string&, const std::filesystem::path&) = 0;
        virtual void update(const std::filesystem::path&) = 0;
    };

    repository(const std::filesystem::path&);
    repository(const repository&);
    ~repository();

    void clone(const std::string&);
    void update();

private:
    std::shared_ptr<driver> _driver;
    std::filesystem::path _directory;
};

}
