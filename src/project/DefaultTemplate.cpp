#include "project/DefaultTemplate.h"

#include "project/ProjectLocator.h"
#include "project/TemplateFile.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace scrap::Project {

namespace {

constexpr std::string_view MainSource = "#include <iostream>\n"
                                        "\n"
                                        "int main()\n"
                                        "{\n"
                                        "    std::cout << \"Hello, world!\\n\";\n"
                                        "}\n";

}  // anonymous namespace

/**
 * The name is written into the manifest as it is: a valid project name holds
 * only letters, digits, '-' and '_', none of which a TOML string escapes.
 */
auto defaultTemplateFiles(std::string_view projectName) -> std::vector<TemplateFile>
{
    std::string manifest = "[package]\nname = \"";
    manifest += projectName;
    manifest += "\"\nversion = \"0.1.0\"\nstd = \"23\"\n";

    std::vector<TemplateFile> files;
    files.push_back(TemplateFile{.path = std::filesystem::path{ManifestFileName}, .content = std::move(manifest)});
    files.push_back(TemplateFile{.path = "src/main.cpp", .content = std::string{MainSource}});
    return files;
}

}  // namespace scrap::Project
