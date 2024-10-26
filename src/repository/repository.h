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
    };

    static repository clone(const repository::type, const std::string&, const std::filesystem::path&);

    repository(const std::filesystem::path&);
    repository(const repository::type, const std::filesystem::path&);
    repository(const repository&);

    void update();

private:
    std::unique_ptr<driver> _driver;
};

}
