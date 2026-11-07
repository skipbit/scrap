#include "toolchain/InstalledToolchain.h"

#include "file/WholeFile.h"
#include "toolchain/HostEnvironment.h"
#include "toolchain/InstallRecord.h"
#include "toolchain/ScrapHomeLayout.h"
#include "toolchain/ToolchainName.h"
#include "toolchain/ToolchainSpecifier.h"
#include "toolchain/ToolchainVersion.h"

#include <algorithm>
#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

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
 * The install record that @p directory holds.
 */
std::expected<InstallRecord, std::string> readRecordFile(const std::filesystem::path& directory)
{
    const std::filesystem::path recordFile = directory / InstallRecordName;
    const auto text = File::readWholeFile(recordFile);
    if (! text.has_value()) {
        return std::unexpected("cannot read '" + recordFile.string() + "'");
    }
    auto record = parseInstallRecord(*text);
    if (! record.has_value()) {
        return std::unexpected("'" + recordFile.string() + "': " + record.error());
    }
    return record;
}

/**
 * Whether what @p record names is there in @p directory.
 *
 * @return Nothing when it is, or what is missing.
 */
std::expected<void, std::string> checkRecordedPaths(const std::filesystem::path& directory, const InstallRecord& record)
{
    const std::filesystem::path recordFile = directory / InstallRecordName;
    const std::filesystem::path compiler = directory / record.compiler;
    std::error_code ec;
    if (! std::filesystem::exists(compiler, ec)) {
        return std::unexpected("'" + recordFile.string() + "' names the compiler '" + compiler.string() + "', which is not there");
    }
    for (const std::filesystem::path& recorded : record.runtimeLibraryDirectories) {
        const std::filesystem::path runtimeLibraryDirectory = directory / recorded;
        if (! std::filesystem::is_directory(runtimeLibraryDirectory, ec)) {
            return std::unexpected("'" + recordFile.string() + "' names the runtime library directory '" + runtimeLibraryDirectory.string() + "', which is not there");
        }
    }
    return {};
}

/**
 * The record in @p directory, when what it names is there.
 */
std::expected<InstallRecord, std::string> readUsableRecord(const std::filesystem::path& directory)
{
    auto record = readRecordFile(directory);
    if (! record.has_value()) {
        return record;
    }
    if (auto checked = checkRecordedPaths(directory, *record); ! checked.has_value()) {
        return std::unexpected(std::move(checked.error()));
    }
    return record;
}

/**
 * The installed toolchain in @p directory, read from its install record.
 */
std::expected<InstalledToolchain, std::string> readInstalled(const std::string& family, const ToolchainVersion& version, const std::filesystem::path& directory)
{
    const auto record = readUsableRecord(directory);
    if (! record.has_value()) {
        return std::unexpected(record.error());
    }
    std::vector<std::filesystem::path> runtimeLibraryDirectories;
    for (const std::filesystem::path& recorded : record->runtimeLibraryDirectories) {
        runtimeLibraryDirectories.push_back(directory / recorded);
    }
    return InstalledToolchain{ .family = family,
                               .version = version,
                               .directory = directory,
                               .compiler = directory / record->compiler,
                               .runtimeLibraryDirectories = std::move(runtimeLibraryDirectories),
                               .arguments = record->arguments };
}

}  // anonymous namespace

std::expected<std::optional<InstallRecord>, std::string> usableInstallRecord(const std::filesystem::path& directory)
{
    if (! isInstalled(directory)) {
        return std::nullopt;
    }
    auto record = readUsableRecord(directory);
    if (! record.has_value()) {
        return std::unexpected(std::move(record.error()));
    }
    return std::move(*record);
}

std::string InstalledToolchain::name() const
{
    return family + "@" + version.text();
}

std::expected<std::optional<InstalledToolchain>, InstalledToolchainFault> findInstalledToolchain(const std::filesystem::path& scrapHome,
                                                                                                 const HostEnvironment& host,
                                                                                                 const ToolchainSpecifier& specifier)
{
    const std::filesystem::path familyPath = familyDirectory(scrapHome, specifier.family);
    std::error_code ec;
    std::filesystem::directory_iterator entries(familyPath, ec);
    if (ec == std::errc::no_such_file_or_directory || ec == std::errc::not_a_directory) {
        return std::nullopt;
    }
    if (ec) {
        return std::unexpected(CannotListToolchains{ .directory = familyPath, .code = ec });
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
            return std::unexpected(CannotListToolchains{ .directory = familyPath, .code = ec });
        }
    }
    if (! newest.has_value()) {
        return std::nullopt;
    }
    auto installed = readInstalled(specifier.family, *newest, newestDirectory);
    if (! installed.has_value()) {
        return std::unexpected(UnusableInstalled{ .reason = std::move(installed.error()) });
    }
    return std::move(*installed);
}

std::expected<InstalledToolchain, InstalledToolchainFault> findInstalledToolchainForHost(const std::filesystem::path& scrapHome,
                                                                                         const ToolchainSpecifier& specifier)
{
    const auto host = hostEnvironment();
    if (! host.has_value()) {
        return std::unexpected(NoToolchainForHost{});
    }
    auto found = findInstalledToolchain(scrapHome, *host, specifier);
    if (! found.has_value()) {
        return std::unexpected(std::move(found.error()));
    }
    std::optional<InstalledToolchain>& toolchain = *found;
    if (! toolchain.has_value()) {
        return std::unexpected(NotInstalled{});
    }
    return std::move(*toolchain);
}

std::optional<InstalledToolchain> anyInstalledToolchain(const std::filesystem::path& scrapHome, const HostEnvironment& host)
{
    std::vector<std::string> families;
    std::error_code ec;
    for (std::filesystem::directory_iterator it(toolchainsDirectory(scrapHome), ec), end; (! ec) && (it != end); it.increment(ec)) {
        const std::string name = it->path().filename().string();
        if (isToolchainName(name)) {
            families.push_back(name);
        }
    }
    std::ranges::sort(families);
    for (const std::string& family : families) {
        const auto found = findInstalledToolchain(scrapHome, host, ToolchainSpecifier{ .family = family, .version = std::nullopt });
        if (! found.has_value()) {
            continue;
        }
        const std::optional<InstalledToolchain>& toolchain = *found;
        if (toolchain.has_value()) {
            return toolchain;
        }
    }
    return std::nullopt;
}

}  // namespace scrap::Toolchain
