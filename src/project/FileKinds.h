#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>

namespace scrap::Project {

/// Extensions of C++ sources.
inline constexpr std::array<std::string_view, 3> CxxSourceExtensions{ ".cpp", ".cc", ".cxx" };

/// Extensions of C sources.
inline constexpr std::array<std::string_view, 1> CSourceExtensions{ ".c" };

namespace Detail {

template <std::size_t A, std::size_t B>
constexpr std::array<std::string_view, A + B> concatenated(const std::array<std::string_view, A>& first, const std::array<std::string_view, B>& second)
{
    std::array<std::string_view, A + B> joined{};
    std::ranges::copy(first, joined.begin());
    std::ranges::copy(second, joined.begin() + A);
    return joined;
}

}  // namespace Detail

/// Extensions a source file is recognised by: those of C++, then those of C.
inline constexpr std::array<std::string_view, 4> SourceExtensions = Detail::concatenated(CxxSourceExtensions, CSourceExtensions);

/// Extensions a header is recognised by.
inline constexpr std::array<std::string_view, 4> HeaderExtensions{ ".h", ".hh", ".hpp", ".hxx" };

}  // namespace scrap::Project
