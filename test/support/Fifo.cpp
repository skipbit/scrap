#include "support/Fifo.h"

#include <gtest/gtest.h>

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <sys/stat.h>

namespace scrap::TestSupport {

std::filesystem::path makeFifoNobodyWrites(const std::filesystem::path& directory)
{
    const std::filesystem::path fifo = directory / "never";
    if (::mkfifo(fifo.c_str(), 0600) != 0) {
        ADD_FAILURE() << "cannot make " << fifo << ": " << std::strerror(errno);
    }
    return fifo;
}

}  // namespace scrap::TestSupport
