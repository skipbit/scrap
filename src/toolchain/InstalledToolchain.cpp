#include "toolchain/InstalledToolchain.h"

#include "file/WholeFile.h"
#include "toolchain/HostEnvironment.h"
#include "toolchain/InstallRecord.h"
#include "toolchain/RuntimeLibraryName.h"
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
 * Why the record in @p directory cannot be used as it is: one written by an
 * earlier scrap names a runtime library directory that holds libc++, and
 * says nothing of what the compiler needs to build with it.
 */
std::expected<void, std::string> checkRecordedArguments(const std::filesystem::path& directory, const InstallRecord& record)
{
    if ((! record.arguments.compile.empty()) || (! record.arguments.link.empty())) {
        return {};
    }
    for (const std::filesystem::path& recorded : record.runtimeLibraryDirectories) {
        const std::filesystem::path runtimeLibraryDirectory = directory / recorded;
        std::error_code ec;
        for (std::filesystem::directory_iterator it(runtimeLibraryDirectory, ec), end; (! ec) && (it != end); it.increment(ec)) {
            if (isLibcxxName(it->path().filename().string())) {
                return std::unexpected("'" + (directory / InstallRecordName).string() + "' was written by an earlier scrap, which did not record how to build with the libc++ in '"
                                       + runtimeLibraryDirectory.string() + "'");
            }
        }
    }
    return {};
}

/**
 * The record in @p directory, when what it names is there and says how to
 * build with it.
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
    if (auto checked = checkRecordedArguments(directory, *record); ! checked.has_value()) {
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

/**
 * A version of a family installed for a host, and the directory it is in.
 */
struct InstalledVersion {
    ToolchainVersion version;
    std::filesystem::path directory;
};

/**
 * The versions of @p family installed below @p scrapHome for @p host, in no
 * particular order. A family that is not there has none.
 */
std::expected<std::vector<InstalledVersion>, CannotListToolchains> installedVersions(const std::filesystem::path& scrapHome,
                                                                                     const HostEnvironment& host,
                                                                                     const std::string& family)
{
    const std::filesystem::path familyPath = familyDirectory(scrapHome, family);
    std::error_code ec;
    std::filesystem::directory_iterator entries(familyPath, ec);
    if (ec == std::errc::no_such_file_or_directory || ec == std::errc::not_a_directory) {
        return std::vector<InstalledVersion>{};
    }
    if (ec) {
        return std::unexpected(CannotListToolchains{ .directory = familyPath, .code = ec });
    }

    std::vector<InstalledVersion> versions;
    // The failure is read straight after the step that caused it, since an
    // increment that fails leaves the iterator at the end.
    for (const std::filesystem::directory_iterator end; entries != end;) {
        std::optional<ToolchainVersion> version = ToolchainVersion::parse(entries->path().filename().string());
        std::filesystem::path directory = entries->path() / host.directoryName();
        if (version.has_value() && isInstalled(directory)) {
            versions.push_back(InstalledVersion{ .version = std::move(*version), .directory = std::move(directory) });
        }
        entries.increment(ec);
        if (ec) {
            return std::unexpected(CannotListToolchains{ .directory = familyPath, .code = ec });
        }
    }
    return versions;
}

/**
 * The families of toolchains installed below @p scrapHome, sorted. None
 * are there when the toolchains directory is not.
 */
std::expected<std::vector<std::string>, CannotListToolchains> installedFamilies(const std::filesystem::path& scrapHome)
{
    const std::filesystem::path directory = toolchainsDirectory(scrapHome);
    std::vector<std::string> families;
    std::error_code ec;
    std::filesystem::directory_iterator entries(directory, ec);
    if (ec == std::errc::no_such_file_or_directory || ec == std::errc::not_a_directory) {
        return families;
    }
    for (const std::filesystem::directory_iterator end; (! ec) && (entries != end); entries.increment(ec)) {
        std::string name = entries->path().filename().string();
        if (isToolchainName(name)) {
            families.push_back(std::move(name));
        }
    }
    if (ec) {
        return std::unexpected(CannotListToolchains{ .directory = directory, .code = ec });
    }
    std::ranges::sort(families);
    return families;
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
    const auto versions = installedVersions(scrapHome, host, specifier.family);
    if (! versions.has_value()) {
        return std::unexpected(versions.error());
    }
    const InstalledVersion* newest = nullptr;
    for (const InstalledVersion& installed : *versions) {
        if (specifier.matches(specifier.family, installed.version) && ((newest == nullptr) || (installed.version > newest->version))) {
            newest = &installed;
        }
    }
    if (newest == nullptr) {
        return std::nullopt;
    }
    auto installed = readInstalled(specifier.family, newest->version, newest->directory);
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
    const auto families = installedFamilies(scrapHome);
    if (! families.has_value()) {
        return std::nullopt;
    }
    for (const std::string& family : *families) {
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

std::optional<std::string> newestNamedBy(const std::vector<InstalledName>& installed, const ToolchainSpecifier& specifier)
{
    const InstalledName* newest = nullptr;
    for (const InstalledName& each : installed) {
        if (specifier.matches(each.family, each.version) && ((newest == nullptr) || (each.version > newest->version))) {
            newest = &each;
        }
    }
    if (newest == nullptr) {
        return std::nullopt;
    }
    return newest->text();
}

std::expected<std::vector<InstalledPlace>, CannotListToolchains> installedNamedBy(const std::filesystem::path& scrapHome,
                                                                                  const HostEnvironment& host,
                                                                                  const ToolchainSpecifier& specifier)
{
    auto versions = installedVersions(scrapHome, host, specifier.family);
    if (! versions.has_value()) {
        return std::unexpected(std::move(versions.error()));
    }
    std::ranges::sort(*versions, {}, &InstalledVersion::version);
    std::vector<InstalledPlace> named;
    for (InstalledVersion& installed : *versions) {
        if (specifier.matches(specifier.family, installed.version)) {
            named.push_back(InstalledPlace{ .name = InstalledName{ .family = specifier.family, .version = std::move(installed.version) },
                                            .directory = std::move(installed.directory) });
        }
    }
    return named;
}

std::string InstalledName::text() const
{
    return family + "@" + version.text();
}

std::expected<std::vector<InstalledName>, CannotListToolchains> listInstalledToolchains(const std::filesystem::path& scrapHome, const HostEnvironment& host)
{
    auto families = installedFamilies(scrapHome);
    if (! families.has_value()) {
        return std::unexpected(std::move(families.error()));
    }
    std::vector<InstalledName> names;
    for (const std::string& family : *families) {
        auto versions = installedVersions(scrapHome, host, family);
        if (! versions.has_value()) {
            return std::unexpected(std::move(versions.error()));
        }
        std::ranges::sort(*versions, {}, &InstalledVersion::version);
        for (InstalledVersion& installed : *versions) {
            names.push_back(InstalledName{ .family = family, .version = std::move(installed.version) });
        }
    }
    return names;
}

}  // namespace scrap::Toolchain
