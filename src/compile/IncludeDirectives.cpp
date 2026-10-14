#include "compile/IncludeDirectives.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace scrap::Compile {

namespace {

/**
 * @p text with each backslash at the end of a line removed together with
 * the line break, as the compiler joins the lines before reading directives.
 */
std::string joinContinuedLines(const std::string_view text)
{
    std::string joined;
    joined.reserve(text.size());
    for (std::size_t index = 0; index < text.size(); ++index) {
        if (text[index] == '\\') {
            if (text.substr(index + 1).starts_with('\n')) {
                ++index;
                continue;
            }
            if (text.substr(index + 1).starts_with("\r\n")) {
                index += 2;
                continue;
            }
        }
        joined += text[index];
    }
    return joined;
}

bool isIdentifierCharacter(const char ch)
{
    return ((ch >= 'a') && (ch <= 'z')) || ((ch >= 'A') && (ch <= 'Z')) || ((ch >= '0') && (ch <= '9')) || (ch == '_');
}

/**
 * The index of the first character at or after @p index in @p text that is
 * neither a blank within the line nor part of a block comment.
 */
std::size_t skipBlanks(const std::string_view text, std::size_t index)
{
    while (index < text.size()) {
        if ((text[index] == ' ') || (text[index] == '\t') || (text[index] == '\v') || (text[index] == '\f') || (text[index] == '\r')) {
            ++index;
        } else if (text.substr(index).starts_with("/*")) {
            const std::size_t end = text.find("*/", index + 2);
            index = (end == std::string_view::npos) ? text.size() : end + 2;
        } else {
            break;
        }
    }
    return index;
}

/**
 * The identifier starting at @p index in @p text.
 */
std::string_view identifierAt(const std::string_view text, const std::size_t index)
{
    std::size_t end = index;
    while ((end < text.size()) && isIdentifierCharacter(text[end])) {
        ++end;
    }
    return text.substr(index, end - index);
}

/**
 * The header name between the quotes or the brackets starting at @p index in
 * @p text, or nothing when none starts there or it runs past the line.
 */
std::optional<std::pair<std::string_view, bool>> headerNameAt(const std::string_view text, const std::size_t index)
{
    if (index >= text.size()) {
        return std::nullopt;
    }
    const char open = text[index];
    if ((open != '"') && (open != '<')) {
        return std::nullopt;
    }
    const std::size_t end = text.find_first_of(open == '"' ? "\"\n" : ">\n", index + 1);
    if ((end == std::string_view::npos) || (text[end] == '\n')) {
        return std::nullopt;
    }
    return std::pair{ text.substr(index + 1, end - index - 1), open == '"' };
}

/**
 * Add to @p directives the name starting at @p index in @p text, looked for
 * as @p next tells, or note that it cannot be read.
 */
void addName(IncludeDirectives& directives, const std::string_view text, const std::size_t index, const bool next)
{
    const auto header = headerNameAt(text, index);
    if (! header.has_value()) {
        directives.computed = true;
        return;
    }
    IncludeKind kind = header->second ? IncludeKind::Quoted : IncludeKind::Angled;
    if (next) {
        kind = IncludeKind::Next;
    }
    directives.names.push_back(IncludedName{ .name = std::filesystem::path{ header->first }, .kind = kind });
}

/**
 * Read the directive whose name starts at @p index in @p text, if it is one
 * that names a header.
 */
void readDirective(IncludeDirectives& directives, const std::string_view text, const std::size_t index)
{
    const std::string_view directive = identifierAt(text, index);
    if ((directive == "include") || (directive == "import") || (directive == "include_next")) {
        addName(directives, text, skipBlanks(text, index + directive.size()), directive == "include_next");
    }
}

/**
 * Read the operator starting at @p index in @p text, if it is one that names
 * a header and is applied.
 */
void readOperator(IncludeDirectives& directives, const std::string_view text, const std::size_t index)
{
    const std::string_view word = identifierAt(text, index);
    if ((word != "__has_include") && (word != "__has_include_next")) {
        return;
    }
    const std::size_t open = skipBlanks(text, index + word.size());
    if ((open < text.size()) && (text[open] == '(')) {
        addName(directives, text, skipBlanks(text, open + 1), word == "__has_include_next");
    }
}

}  // anonymous namespace

IncludeDirectives parseIncludeDirectives(const std::string_view text)
{
    const std::string joined = joinContinuedLines(text);
    const std::string_view source = joined;

    IncludeDirectives directives;
    for (std::size_t index = 0; index < source.size(); ++index) {
        if (source[index] == '#') {
            readDirective(directives, source, skipBlanks(source, index + 1));
        } else if (source.substr(index).starts_with("%:")) {
            readDirective(directives, source, skipBlanks(source, index + 2));
        } else if (source.substr(index).starts_with("__has_include") && ((index == 0) || (! isIdentifierCharacter(source[index - 1])))) {
            readOperator(directives, source, index);
        }
    }
    return directives;
}

}  // namespace scrap::Compile
