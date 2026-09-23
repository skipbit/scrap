#pragma once

#include "project/LanguageStandard.h"
#include "toolchain/CompilerIdentity.h"

#include <optional>
#include <string>

namespace scrap::Compile {

/**
 * @brief How one compiler, of one family and version, is told what to do.
 *
 * The same request is spelled differently across compilers and their
 * versions: C++23 is -std=c++23 to clang 17 and -std=c++2b to clang 16. The
 * driver knows the spellings its compiler takes, as CMake's compiler tables
 * record them.
 */
class CompilerDriver {
public:
    explicit CompilerDriver(Toolchain::CompilerIdentity identity);

    /**
     * @brief The option that selects @p standard.
     *
     * A compiler whose family is unknown is given the standard's own name,
     * and whether it takes it is left to the compiler to say.
     *
     * @return The option, or nothing when this version cannot build the
     *         standard.
     */
    [[nodiscard]] std::optional<std::string> standardOption(Project::LanguageStandard standard) const;

    /**
     * @brief The option that keeps colour in diagnostics read through a pipe.
     *
     * @return The option, or nothing for a compiler whose family is unknown,
     *         which might reject it.
     */
    [[nodiscard]] std::optional<std::string> colorOption() const;

private:
    Toolchain::CompilerIdentity _identity;
};

/**
 * @brief The option naming @p standard as the standard itself is named, such
 *        as -std=c++23.
 */
[[nodiscard]] std::string standardNameOption(Project::LanguageStandard standard);

}  // namespace scrap::Compile
