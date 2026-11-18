#include "command/TextColumns.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace scrap::Command {

namespace {

constexpr std::string_view ColumnGap = "  ";

}  // anonymous namespace

std::string layOutColumns(const std::vector<std::vector<std::string>>& rows)
{
    std::vector<std::size_t> widths(rows.front().size(), 0);
    for (const auto& row : rows) {
        for (std::size_t column = 0; column < row.size(); ++column) {
            widths[column] = std::max(widths[column], row[column].size());
        }
    }
    std::string text;
    for (const auto& row : rows) {
        std::string line;
        for (std::size_t column = 0; column < row.size(); ++column) {
            if (column > 0) {
                line += ColumnGap;
            }
            line += row[column];
            line.append(widths[column] - row[column].size(), ' ');
        }
        line.erase(line.find_last_not_of(' ') + 1);
        text += line + "\n";
    }
    return text;
}

}  // namespace scrap::Command
