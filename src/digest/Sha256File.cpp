#include "digest/Sha256File.h"

#include "digest/Sha256.h"

#include <cerrno>
#include <cstddef>
#include <expected>  // IWYU pragma: keep
#include <fcntl.h>
#include <filesystem>
#include <string>
#include <string_view>
#include <sys/types.h>
#include <system_error>
#include <unistd.h>
#include <vector>

namespace scrap::Digest {

namespace {

constexpr std::size_t ReadSize = std::size_t{ 1 } << 20U;

std::error_code lastFailure()
{
    return { errno, std::generic_category() };
}

/// Closes the descriptor it holds.
class OpenFile {
public:
    explicit OpenFile(const int descriptor)
        : _descriptor{ descriptor }
    {
    }
    OpenFile(const OpenFile&) = delete;
    OpenFile& operator=(const OpenFile&) = delete;
    OpenFile(OpenFile&&) = delete;
    OpenFile& operator=(OpenFile&&) = delete;
    ~OpenFile()
    {
        ::close(_descriptor);
    }

    [[nodiscard]] int descriptor() const
    {
        return _descriptor;
    }

private:
    int _descriptor;
};

}  // anonymous namespace

std::expected<std::string, std::error_code> sha256OfFile(const std::filesystem::path& file)
{
    const int descriptor = ::open(file.c_str(), O_RDONLY | O_CLOEXEC);
    if (descriptor < 0) {
        return std::unexpected(lastFailure());
    }
    const OpenFile opened{ descriptor };

    Sha256 sha256;
    std::vector<char> buffer(ReadSize);
    while (true) {
        const ssize_t received = ::read(opened.descriptor(), buffer.data(), buffer.size());
        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }
            return std::unexpected(lastFailure());
        }
        if (received == 0) {
            break;
        }
        sha256.update(std::string_view{ buffer.data(), static_cast<std::size_t>(received) });
    }
    return sha256.hexDigest();
}

}  // namespace scrap::Digest
