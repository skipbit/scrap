#include "format/ClangFormat.h"

#include "file/WholeFile.h"
#include "support/TempDirectory.h"

#include <gtest/gtest.h>

#include <csignal>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <variant>
#include <vector>

using namespace scrap::Format;
using scrap::TestSupport::TempDirectory;

namespace {

/**
 * A clang-format that writes the arguments it was given, one to a line, to
 * the file "arguments" in the directory it runs in, and then runs @p rest.
 */
ClangFormat recordingTool(const TempDirectory& temp, const std::string& rest = "", const std::string& directory = "tool")
{
    const auto program = temp.writeScript(directory + "/clang-format", "printf '%s\\n' \"$@\" > arguments\n" + rest);
    return ClangFormat{ .program = program, .version = "18.1.3" };
}

/// The lines of @p file, without their newlines.
std::vector<std::string> linesOf(const std::filesystem::path& file)
{
    const auto text = scrap::File::readWholeFile(file);
    if (! text.has_value()) {
        ADD_FAILURE() << "cannot read " << file;
        return {};
    }
    std::vector<std::string> lines;
    std::string::size_type start = 0;
    while (start < text->size()) {
        const auto end = text->find('\n', start);
        lines.push_back(text->substr(start, end - start));
        start = (end == std::string::npos) ? text->size() : (end + 1);
    }
    return lines;
}

}  // namespace

/**
 * clang-format is looked for on the search paths in order, and the first
 * that can be run is returned; nothing comes back when none holds it.
 */
TEST(ClangFormatTest, FindsTheFirstClangFormatOnTheSearchPaths)
{
    const TempDirectory temp;
    temp.makeDirectory("empty");
    temp.writeScript("first/clang-format", "exit 0\n");
    temp.writeScript("second/clang-format", "exit 0\n");

    const auto found = findClangFormat({ temp.path() / "empty", temp.path() / "first", temp.path() / "second" });

    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(*found, temp.path() / "first" / "clang-format");
    EXPECT_FALSE(findClangFormat({ temp.path() / "empty" }).has_value());
}

/**
 * The version is the word after "version" in the first line --version
 * writes, whoever packaged the program.
 */
TEST(ClangFormatTest, ReadsTheVersionAfterTheWordVersion)
{
    const TempDirectory temp;
    const auto ubuntu = temp.writeScript("ubuntu/clang-format", "[ \"$1\" = --version ] || exit 1\necho 'Ubuntu clang-format version 18.1.3 (1ubuntu1)'\necho 'second line'\n");
    const auto homebrew = temp.writeScript("homebrew/clang-format", "echo 'Homebrew clang-format version 22.1.8'\n");

    const auto fromUbuntu = describeClangFormat(ubuntu);
    const auto fromHomebrew = describeClangFormat(homebrew);

    ASSERT_TRUE(fromUbuntu.has_value());
    EXPECT_EQ(fromUbuntu->program, ubuntu);
    EXPECT_EQ(fromUbuntu->version, "18.1.3");
    ASSERT_TRUE(fromHomebrew.has_value());
    EXPECT_EQ(fromHomebrew->version, "22.1.8");
}

/**
 * A first line without the word "version" is kept whole, so something is
 * still shown of what the program says it is.
 */
TEST(ClangFormatTest, KeepsTheWholeFirstLineWithoutTheWordVersion)
{
    const TempDirectory temp;
    const auto program = temp.writeScript("tool/clang-format", "echo 'a formatter, release 2'\n");

    const auto described = describeClangFormat(program);

    ASSERT_TRUE(described.has_value());
    EXPECT_EQ(described->version, "a formatter, release 2");
}

/**
 * A program that cannot be started, or that fails to report its version, is
 * reported as such.
 */
TEST(ClangFormatTest, ReportsAVersionItCannotLearn)
{
    const TempDirectory temp;
    const auto failing = temp.writeScript("tool/clang-format", "exit 3\n");

    const auto notStarted = describeClangFormat(temp.path() / "missing" / "clang-format");
    const auto failed = describeClangFormat(failing);

    ASSERT_FALSE(notStarted.has_value());
    EXPECT_TRUE(std::holds_alternative<ToolNotRun>(notStarted.error()));
    ASSERT_FALSE(failed.has_value());
    ASSERT_TRUE(std::holds_alternative<ToolFailed>(failed.error()));
    EXPECT_EQ(std::get<ToolFailed>(failed.error()).exitCode, std::optional<int>{ 3 });
}

/**
 * A --version that fails reports what it wrote to standard error as well.
 */
TEST(ClangFormatTest, ReportsWhyAVersionFailed)
{
    const TempDirectory temp;
    const auto failing = temp.writeScript("tool/clang-format", "echo 'no version for you' >&2\nexit 2\n");

    const auto described = describeClangFormat(failing);

    ASSERT_FALSE(described.has_value());
    ASSERT_TRUE(std::holds_alternative<ToolFailed>(described.error()));
    const auto& failure = std::get<ToolFailed>(described.error());
    EXPECT_EQ(failure.exitCode, std::optional<int>{ 2 });
    EXPECT_EQ(failure.output, "no version for you\n");
}

/**
 * A format runs clang-format in the project root with -i, the style when one
 * is given, and the file's path relative to the root.
 */
TEST(ClangFormatTest, FormatsAFileInPlaceFromTheProjectRoot)
{
    const TempDirectory temp;
    const ClangFormat tool = recordingTool(temp);
    const std::filesystem::path root = temp.makeDirectory("project");

    ASSERT_TRUE(formatFile(tool, root, "src/a.cpp", std::nullopt).has_value());
    EXPECT_EQ(linesOf(root / "arguments"), (std::vector<std::string>{ "-i", "src/a.cpp" }));

    ASSERT_TRUE(formatFile(tool, root, "src/a.cpp", "{BasedOnStyle: llvm, IndentWidth: 4}").has_value());
    EXPECT_EQ(linesOf(root / "arguments"), (std::vector<std::string>{ "-i", "--style={BasedOnStyle: llvm, IndentWidth: 4}", "src/a.cpp" }));
}

/**
 * A file whose name starts with '-' or '@' is passed with a leading ./, so
 * clang-format does not read it as an option or a file of options.
 */
TEST(ClangFormatTest, PassesAFileThatLooksLikeAnOptionAsAFile)
{
    const TempDirectory temp;
    const ClangFormat tool = recordingTool(temp);
    const std::filesystem::path root = temp.makeDirectory("project");

    ASSERT_TRUE(formatFile(tool, root, "-x.cpp", std::nullopt).has_value());
    EXPECT_EQ(linesOf(root / "arguments"), (std::vector<std::string>{ "-i", "./-x.cpp" }));

    ASSERT_TRUE(formatFile(tool, root, "@x.cpp", std::nullopt).has_value());
    EXPECT_EQ(linesOf(root / "arguments"), (std::vector<std::string>{ "-i", "./@x.cpp" }));
}

/**
 * A format that fails comes back with how clang-format ended and everything
 * it wrote, standard error included.
 */
TEST(ClangFormatTest, ReportsWhatAFailedFormatWrote)
{
    const TempDirectory temp;
    const ClangFormat failing = recordingTool(temp, "echo 'Invalid value for style' >&2\nexit 1\n");
    const std::filesystem::path root = temp.makeDirectory("project");

    const auto formatted = formatFile(failing, root, "src/a.cpp", "nonsense");

    ASSERT_FALSE(formatted.has_value());
    ASSERT_TRUE(std::holds_alternative<ToolFailed>(formatted.error()));
    const auto& failure = std::get<ToolFailed>(formatted.error());
    EXPECT_EQ(failure.exitCode, std::optional<int>{ 1 });
    EXPECT_FALSE(failure.signal.has_value());
    EXPECT_EQ(failure.output, "Invalid value for style\n");
}

/**
 * A format stopped by a signal is reported with the signal.
 */
TEST(ClangFormatTest, ReportsAFormatStoppedByASignal)
{
    const TempDirectory temp;
    const ClangFormat killed = recordingTool(temp, "kill -KILL $$\n");
    const std::filesystem::path root = temp.makeDirectory("project");

    const auto formatted = formatFile(killed, root, "src/a.cpp", std::nullopt);

    ASSERT_FALSE(formatted.has_value());
    ASSERT_TRUE(std::holds_alternative<ToolFailed>(formatted.error()));
    EXPECT_EQ(std::get<ToolFailed>(formatted.error()).signal, std::optional<int>{ SIGKILL });
    EXPECT_FALSE(std::get<ToolFailed>(formatted.error()).exitCode.has_value());
}

/**
 * A check compares what clang-format writes with the file, which it leaves as
 * it is, and passes the style without -i.
 */
TEST(ClangFormatTest, ChecksAFileWithoutChangingIt)
{
    const TempDirectory temp;
    const std::filesystem::path root = temp.makeDirectory("project");
    temp.writeFile("project/src/a.cpp", "int a;\n");
    const ClangFormat echoing = recordingTool(temp, "for last; do :; done\ncat \"$last\"\n", "echoing");
    const ClangFormat changing = recordingTool(temp, "echo 'int  a;'\n", "changing");

    const auto formatted = checkFile(echoing, root, "src/a.cpp", "llvm");
    ASSERT_TRUE(formatted.has_value());
    EXPECT_EQ(*formatted, FileState::Formatted);
    EXPECT_EQ(linesOf(root / "arguments"), (std::vector<std::string>{ "--style=llvm", "src/a.cpp" }));

    const auto unformatted = checkFile(changing, root, "src/a.cpp", std::nullopt);
    ASSERT_TRUE(unformatted.has_value());
    EXPECT_EQ(*unformatted, FileState::Unformatted);
    EXPECT_EQ(linesOf(root / "arguments"), (std::vector<std::string>{ "src/a.cpp" }));
    EXPECT_EQ(linesOf(root / "src" / "a.cpp"), (std::vector<std::string>{ "int a;" }));
}

/**
 * A check that fails reports what clang-format wrote to standard error,
 * which the formatted text is read without.
 */
TEST(ClangFormatTest, ReportsWhyACheckFailed)
{
    const TempDirectory temp;
    const std::filesystem::path root = temp.makeDirectory("project");
    const ClangFormat failing = recordingTool(temp, "echo 'cannot parse the style' >&2\nexit 1\n");

    const auto checked = checkFile(failing, root, "src/a.cpp", "nonsense");

    ASSERT_FALSE(checked.has_value());
    ASSERT_TRUE(std::holds_alternative<ToolFailed>(checked.error()));
    EXPECT_EQ(std::get<ToolFailed>(checked.error()).exitCode, std::optional<int>{ 1 });
    EXPECT_EQ(std::get<ToolFailed>(checked.error()).output, "cannot parse the style\n");
}

/**
 * A check whose clang-format succeeds on a file that is not there to read
 * back reports that it could not compare.
 */
TEST(ClangFormatTest, ReportsAFileACheckCannotReadBack)
{
    const TempDirectory temp;
    const std::filesystem::path root = temp.makeDirectory("project");
    const ClangFormat succeeding = recordingTool(temp, "echo 'int a;'\n");

    const auto checked = checkFile(succeeding, root, "src/missing.cpp", std::nullopt);

    ASSERT_FALSE(checked.has_value());
    EXPECT_TRUE(std::holds_alternative<FileUnreadable>(checked.error()));
}
