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
        if ((ch == '\\') && (next == '\n')) {
            endWord();
            ++index;
        } else if ((ch == '\\') && ((next == ' ') || (next == '#'))) {
            word += next;
            ++index;
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
