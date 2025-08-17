#include "helpers/TestPresenter.h"
#include <catch2/catch_test_macros.hpp>
#include <memory>

using namespace scrap::test;

// Simple test without Operation class for now
class SimpleTestClass {
public:
    static bool execute(scrap::Presenter& presenter, const std::vector<std::string>& args)
    {
        presenter.displayInfo("Test executed with " + std::to_string(args.size()) + " arguments");
        return true;
    }
};

TEST_CASE("Basic test functionality", "[basic]")
{
    auto presenter = std::make_unique<TestPresenter>();

    SECTION("simple test execution")
    {
        std::vector<std::string> args = {"arg1", "arg2"};

        bool result = SimpleTestClass::execute(*presenter, args);

        REQUIRE(result == true);
        REQUIRE(presenter->hasOutput());
        REQUIRE(presenter->output().find("Test executed with 2 arguments") != std::string::npos);
    }

    SECTION("test with no arguments")
    {
        std::vector<std::string> args;

        bool result = SimpleTestClass::execute(*presenter, args);

        REQUIRE(result == true);
        REQUIRE(presenter->hasOutput());
        REQUIRE(presenter->output().find("Test executed with 0 arguments") != std::string::npos);
    }

    SECTION("presenter output functionality")
    {
        presenter->displayInfo("test info");
        presenter->displayError("test error");
        presenter->displayWarning("test warning");

        REQUIRE(presenter->hasOutput());
        REQUIRE(presenter->hasErrors());
        REQUIRE(presenter->output().find("INFO: test info") != std::string::npos);
        REQUIRE(presenter->output().find("WARNING: test warning") != std::string::npos);
        REQUIRE(presenter->errorOutput().find("ERROR: test error") != std::string::npos);
    }
}
