#include "toolchain/InstalledToolchain.h"

#include "file/WholeFile.h"
#include "toolchain/HostEnvironment.h"
#include "toolchain/InstallRecord.h"
#include "toolchain/ToolchainSpecifier.h"
#include "toolchain/ToolchainVersion.h"

#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>

namespace scrap::Toolchain {

namespace {

/**
 * Whether @p directory holds a toolchain installed to the end.
 */
bool isInstalled(const std::filesystem::path& directory)
{
    std::error_code ec;
    return std::filesystem::exists(directory / InstallRecordName, ec);
}

/**
 * The installed toolchain in @p directory, read from its install record.
 */
std::expected<InstalledToolchain, std::string> readInstalled(const std::string& family, const ToolchainVersion& version, const std::filesystem::path& directory)
{
    const std::filesystem::path recordFile = directory / InstallRecordName;
    auto text = File::readWholeFile(recordFile);
    if (! text.has_value()) {
        return std::unexpected("cannot read '" + recordFile.string() + "'");
    }
    auto record = parseInstallRecord(*text);
    if (! record.has_value()) {
        return std::unexpected("'" + recordFile.string() + "': " + record.error());
    }
    const std::filesystem::path compiler = directory / record->compiler;
    std::error_code ec;
    if (! std::filesystem::exists(compiler, ec)) {
        return std::unexpected("'" + recordFile.string() + "' names the compiler '" + compiler.string() + "', which is not there");
    }
    return InstalledToolchain{ .family = family, .version = version, .directory = directory, .compiler = compiler };
}

}  // anonymous namespace

std::expected<std::optional<InstalledToolchain>, std::string> findInstalledToolchain(const std::filesystem::path& scrapHome,
                                                                                     const HostEnvironment& host,
                                                                                     const ToolchainSpecifier& specifier)
{
    const std::filesystem::path familyDirectory = scrapHome / "toolchains" / specifier.family;
    std::error_code ec;
    std::filesystem::directory_iterator entries(familyDirectory, ec);
    if (ec == std::errc::no_such_file_or_directory || ec == std::errc::not_a_directory) {
        return std::nullopt;
    }
    if (ec) {
        return std::unexpected("cannot list '" + familyDirectory.string() + "': " + ec.message());
    }

    std::optional<ToolchainVersion> newest;
    std::filesystem::path newestDirectory;
    // The failure is read straight after the step that caused it, since an
    // increment that fails leaves the iterator at the end.
    for (const std::filesystem::directory_iterator end; entries != end;) {
        const std::optional<ToolchainVersion> version = ToolchainVersion::parse(entries->path().filename().string());
        const std::filesystem::path directory = entries->path() / host.directoryName();
        if (version.has_value() && specifier.matches(specifier.family, *version) && isInstalled(directory)
            && (! newest.has_value() || (*version > *newest))) {
            newest = version;
            newestDirectory = directory;
        }
        entries.increment(ec);
        if (ec) {
            return std::unexpected("cannot list '" + familyDirectory.string() + "': " + ec.message());
        }
    }
    if (! newest.has_value()) {
        return std::nullopt;
    }
    return readInstalled(specifier.family, *newest, newestDirectory);
}

}  // namespace scrap::Toolchain
