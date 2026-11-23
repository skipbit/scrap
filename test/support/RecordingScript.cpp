#include "support/RecordingScript.h"

#include <gtest/gtest.h>

#include <fstream>
#include <iterator>

namespace scrap::TestSupport {

std::filesystem::path writeRecordingScript(const TempDirectory& temp, std::string_view relative, std::string_view rest)
{
    std::string script = "printf '%s\\n' \"$@\" > arguments\n";
    script += rest;
    return temp.writeScript(relative, script);
}

std::vector<std::string> linesOf(const std::filesystem::path& file)
{
    std::ifstream in{ file, std::ios::binary };
    if (! in) {
        ADD_FAILURE() << "cannot read " << file;
        return {};
    }
    const std::string text{ std::istreambuf_iterator<char>{ in }, std::istreambuf_iterator<char>{} };
    std::vector<std::string> lines;
    std::string::size_type start = 0;
    while (start < text.size()) {
        const auto end = text.find('\n', start);
        lines.push_back(text.substr(start, end - start));
        start = (end == std::string::npos) ? text.size() : (end + 1);
    }
    return lines;
}

}  // namespace scrap::TestSupport
