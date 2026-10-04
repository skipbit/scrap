// Uses one part of each dross module, so that linking pulls each of them out
// of the archive, and fails when a result is not the expected one.

#include <atomic>
#include <dross/dross.h>
#include <iostream>
#include <string>
#include <version>

namespace {

int failures = 0;

void expect(const bool holds, const std::string& what)
{
    if (! holds) {
        std::cerr << "unexpected: " << what << '\n';
        ++failures;
    }
}

void checkType()
{
    const dross::number sum = dross::number{ "12345678901234567890" } + dross::number{ "1" };
    expect(std::string{ sum } == "12345678901234567891", "type: number addition");

    const dross::array list = { dross::value{ 1 }, dross::value{ "two" }, dross::value{ 3.0 } };
    expect(list.length() == 3, "type: array length");
}

void checkFormat()
{
    const auto parsed = dross::toml::deserialize(dross::data{ "name = \"dross\"\ncount = 3" });
    expect(parsed.has_value(), "format: toml parses");
    if (parsed.has_value()) {
        const dross::value& name = (*parsed)["name"];
        expect(name.is<dross::string>() && (name.as<dross::string>() == "dross"), "format: toml string");
        const dross::value& count = (*parsed)["count"];
        expect(count.is<dross::number>() && (count.as<dross::number>() == 3), "format: toml integer");
    }
}

void checkPlatform()
{
    const dross::path joined = dross::path{ std::string{ "a" } }.append("b");
    expect(joined.string() == "a/b", "platform: path append");
}

void checkThread()
{
    std::atomic<int> ran{ 0 };
    dross::thread worker{ [&ran] {
        ran = 1;
    } };
    worker.join();
    expect(ran == 1, "thread: body ran before join returned");
}

// The macro the standard library defines, in the form the workflow's matrix
// names it.
void reportStandardLibrary()
{
#if defined(_LIBCPP_VERSION)
    std::cout << "standard library: _LIBCPP_VERSION " << _LIBCPP_VERSION << '\n';
#elif defined(_GLIBCXX_RELEASE)
    std::cout << "standard library: _GLIBCXX_RELEASE " << _GLIBCXX_RELEASE << '\n';
#else
    std::cout << "standard library: unknown\n";
#endif
}

}  // anonymous namespace

int main()
{
    reportStandardLibrary();
    checkType();
    checkFormat();
    checkPlatform();
    checkThread();
    if (failures != 0) {
        return 1;
    }
    std::cout << "dross works\n";
    return 0;
}
