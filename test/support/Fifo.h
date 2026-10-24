#pragma once

#include <filesystem>

namespace scrap::TestSupport {

/**
 * @brief Create in @p directory a FIFO named never that nothing ever writes
 * to, so a program that reads it waits until it is stopped.
 *
 * Reports a test failure if the FIFO cannot be made, so a broken fixture is
 * not mistaken for a failure of the code under test.
 *
 * @param directory Where the FIFO is made.
 * @return Full path of the FIFO.
 */
std::filesystem::path makeFifoNobodyWrites(const std::filesystem::path& directory);

}  // namespace scrap::TestSupport
