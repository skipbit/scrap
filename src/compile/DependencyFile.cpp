#include "compile/DependencyFile.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace scrap::Compile {

namespace {

bool separatesWords(const char ch)
{
    return (ch == ' ') || (ch == '\t') || (ch == '\n') || (ch == '\r');
}

/**
 * Undo the escape that the backslashes starting at @p index in @p text begin,
 * adding what they stand for to @p word, and return the index of the last
 * character they take.
 *
 * Before a space or a tab, GCC doubles each backslash and adds one, so an odd
 * count escapes the blank and an even one ends the word after half of them.
 * A single backslash before a '#' escapes it, and one before a line break
 * continues the line, which ends the word as the line break would. Backslashes
 * before anything else are part of the name.
 */
std::size_t takeBackslashes(const std::string_view text, const std::size_t index, std::string& word)
{
    const std::size_t end = std::min(text.find_first_not_of('\\', index), text.size());
    const std::size_t count = end - index;
    const char after = (end < text.size()) ? text[end] : '\0';
    if ((after == ' ') || (after == '\t')) {
        word.append(count / 2, '\\');
        if ((count % 2) == 0) {
            return end - 1;
        }
        word += after;
        return end;
    }
    if (after == '#') {
        word.append(count - 1, '\\');
        word += '#';
        return end;
    }
    if (after == '\n') {
        word.append(count - 1, '\\');
        return end - 1;
    }
    word.append(count, '\\');
    return end - 1;
}

/**
 * The words of @p text, with the escapes undone and the lines a backslash
 * continues joined.
 */
std::vector<std::string> wordsOf(const std::string_view text)
{
    std::vector<std::string> words;
    std::string word;
    const auto endWord = [&] {
        if (! word.empty()) {
            words.push_back(std::move(word));
            word.clear();
        }
    };
    for (std::size_t index = 0; index < text.size(); ++index) {
        const char ch = text[index];
        const char next = (index + 1 < text.size()) ? text[index + 1] : '\0';
        if (ch == '\\') {
            index = takeBackslashes(text, index, word);
        } else if ((ch == '$') && (next == '$')) {
            word += '$';
            ++index;
        } else if (separatesWords(ch)) {
            endWord();
        } else {
            word += ch;
        }
    }
    endWord();
    return words;
}

bool namesTarget(const std::string& word)
{
    return word.ends_with(':');
}

}  // anonymous namespace

std::optional<std::vector<std::filesystem::path>> parseDependencyFile(const std::string_view text)
{
    const std::vector<std::string> words = wordsOf(text);
    if (words.empty() || (! namesTarget(words.front())) || (std::ranges::find(words, ":") != words.end())) {
        return std::nullopt;
    }
    // What follows the files of the first rule can only be the rules -MP
    // adds, each a header named as a target with nothing after it.
    const auto files = std::ranges::find_if(words.begin() + 1, words.end(), namesTarget);
    if (! std::ranges::all_of(files, words.end(), namesTarget)) {
        return std::nullopt;
    }
    return std::vector<std::filesystem::path>(words.begin() + 1, files);
}

}  // namespace scrap::Compile
