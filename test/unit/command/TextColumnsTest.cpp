#include "command/TextColumns.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace scrap::Command;

/**
 * Each column is as wide as its widest cell, two spaces apart, and no line
 * ends in spaces, even where its last cells are empty.
 */
TEST(TextColumnsTest, LaysOutEachColumnAsWideAsItsWidestCell)
{
    const std::vector<std::vector<std::string>> rows{ { "TOOLCHAIN", "STATUS", "INSTALLED" },
                                                      { "gcc@15.2.0", "active", "yes" },
                                                      { "llvm@22.1.8", "deprecated", "" } };

    EXPECT_EQ(layOutColumns(rows),
              "TOOLCHAIN    STATUS      INSTALLED\n"
              "gcc@15.2.0   active      yes\n"
              "llvm@22.1.8  deprecated\n");
}
