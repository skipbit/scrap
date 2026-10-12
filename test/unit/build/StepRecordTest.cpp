#include "build/StepRecord.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

using namespace scrap::Build;

namespace {

StepRecord sampleRecord()
{
    return StepRecord{
        .arguments = { "/usr/bin/c++", "-c", "src/main.cpp", "-o", "build/debug/obj/hello/src/main.cpp.o" },
        .inputs = {
            InputStamp{ .path = "src/main.cpp", .modified = 1791770989123456789, .size = 120 },
            InputStamp{ .path = "/usr/include/c++/13/iostream", .modified = 1700000000000000000, .size = 3021 },
        },
    };
}

}  // anonymous namespace

/**
 * A record is written a line per item, between a line naming the format and
 * a line counting the items.
 */
TEST(StepRecordTest, RendersALinePerItem)
{
    const StepRecord record{
        .arguments = { "c++", "-c", "a.cpp" },
        .inputs = { InputStamp{ .path = "a.cpp", .modified = 42, .size = 7 } },
    };

    EXPECT_EQ(renderStepRecord(record),
              "scrap-step-record 1\n"
              "argument c++\n"
              "argument -c\n"
              "argument a.cpp\n"
              "input 42 7 a.cpp\n"
              "end 3 1\n");
}

/**
 * What is written reads back as the same record.
 */
TEST(StepRecordTest, ReadsBackWhatItWrote)
{
    const StepRecord record = sampleRecord();

    EXPECT_EQ(parseStepRecord(renderStepRecord(record)), record);
}

/**
 * A backslash or a line break within an argument or a path is kept, and the
 * item stays on a line of its own.
 */
TEST(StepRecordTest, KeepsBackslashesAndLineBreaks)
{
    const StepRecord record{
        .arguments = { "c++", R"(-DTEXT="a\nb")", "line\nbreak", "\\", "" },
        .inputs = { InputStamp{ .path = "dir\nwith\\odd name.cpp", .modified = 1, .size = 2 } },
    };

    const std::string text = renderStepRecord(record);

    EXPECT_EQ(text,
              "scrap-step-record 1\n"
              "argument c++\n"
              "argument -DTEXT=\"a\\\\nb\"\n"
              "argument line\\nbreak\n"
              "argument \\\\\n"
              "argument \n"
              "input 1 2 dir\\nwith\\\\odd name.cpp\n"
              "end 5 1\n");
    EXPECT_EQ(parseStepRecord(text), record);
}

/**
 * A record without arguments or inputs, and the extremes of each number, read
 * back as written.
 */
TEST(StepRecordTest, ReadsBackEmptyListsAndExtremeNumbers)
{
    const StepRecord empty{};
    const StepRecord extremes{
        .arguments = { "c++" },
        .inputs = {
            InputStamp{ .path = "old", .modified = std::numeric_limits<std::int64_t>::min(), .size = 0 },
            InputStamp{ .path = "big", .modified = std::numeric_limits<std::int64_t>::max(), .size = std::numeric_limits<std::uintmax_t>::max() },
        },
    };

    EXPECT_EQ(parseStepRecord(renderStepRecord(empty)), empty);
    EXPECT_EQ(parseStepRecord(renderStepRecord(extremes)), extremes);
}

/**
 * A record that is not whole, or not of this version, is not read as one.
 */
TEST(StepRecordTest, RejectsWhatIsNotAWholeRecord)
{
    const std::string whole = renderStepRecord(sampleRecord());
    ASSERT_TRUE(parseStepRecord(whole).has_value());

    const std::string withoutEnd = whole.substr(0, whole.rfind("end "));
    const std::string withoutLastInput = whole.substr(0, whole.rfind("input ")) + "end 5 2\n";

    struct Rejected {
        const char* reason;
        std::string text;
    };
    const std::vector<Rejected> rejected = {
        { "empty", "" },
        { "only a line break", "\n" },
        { "header only", "scrap-step-record 1\n" },
        { "no final line break", whole.substr(0, whole.size() - 1) },
        { "cut before the end line", withoutEnd },
        { "a line lost before the end line", withoutLastInput },
        { "another version", "scrap-step-record 2\nargument c++\nend 1 0\n" },
        { "argument count differs", "scrap-step-record 1\nargument c++\nend 2 0\n" },
        { "line after the end line", "scrap-step-record 1\nargument c++\nend 1 0\nargument c++\n" },
        { "blank line after the end line", "scrap-step-record 1\nargument c++\nend 1 0\n\n" },
        { "argument after an input", "scrap-step-record 1\ninput 1 2 a\nargument c++\nend 1 1\n" },
        { "unknown escape", "scrap-step-record 1\nargument a\\tb\nend 1 0\n" },
        { "trailing backslash", "scrap-step-record 1\nargument a\\\nend 1 0\n" },
        { "modified time not a number", "scrap-step-record 1\ninput x 2 a\nend 0 1\n" },
        { "plus sign", "scrap-step-record 1\ninput +1 2 a\nend 0 1\n" },
        { "negative size", "scrap-step-record 1\ninput 1 -2 a\nend 0 1\n" },
        { "no path", "scrap-step-record 1\ninput 1 2\nend 0 1\n" },
        { "modified time overflows", "scrap-step-record 1\ninput 99999999999999999999 2 a\nend 0 1\n" },
        { "space after the last count", "scrap-step-record 1\nargument c++\nend 1 0 \n" },
        { "one count only", "scrap-step-record 1\nargument c++\nend 1\n" },
        { "unknown tag", "scrap-step-record 1\nother c++\nend 1 0\n" },
        { "carriage return after the header", "scrap-step-record 1\r\nargument c++\nend 1 0\n" },
    };
    for (const Rejected& each : rejected) {
        EXPECT_EQ(parseStepRecord(each.text), std::nullopt) << "reason: " << each.reason << "\ntext: " << each.text;
    }
}
