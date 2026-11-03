#include "xz/XzFile.h"

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <expected>  // IWYU pragma: keep
#include <fcntl.h>
#include <filesystem>
#include <functional>
#include <lzma.h>
#include <string_view>
#include <sys/types.h>
#include <system_error>
#include <unistd.h>
#include <vector>

namespace scrap::Xz {

namespace {

constexpr std::size_t BufferSize = std::size_t{ 1 } << 20U;
constexpr std::uint64_t ShareOfMemoryForThreads = 4;

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

/// Ends the decoder it holds.
class Decoder {
public:
    Decoder() = default;
    Decoder(const Decoder&) = delete;
    Decoder& operator=(const Decoder&) = delete;
    Decoder(Decoder&&) = delete;
    Decoder& operator=(Decoder&&) = delete;
    ~Decoder()
    {
        lzma_end(&stream);
    }

    lzma_stream stream{};
};

ssize_t readRetrying(const int descriptor, std::vector<std::uint8_t>& buffer)
{
    while (true) {
        const ssize_t received = ::read(descriptor, buffer.data(), buffer.size());
        if (received >= 0 || errno != EINTR) {
            return received;
        }
    }
}

lzma_ret startDecoder(lzma_stream& stream)
{
    lzma_mt options{};
    options.flags = LZMA_CONCATENATED | LZMA_TELL_UNSUPPORTED_CHECK;
    options.threads = std::max<std::uint32_t>(lzma_cputhreads(), 1);
    options.memlimit_threading = lzma_physmem() / ShareOfMemoryForThreads;
    options.memlimit_stop = UINT64_MAX;
    return lzma_stream_decoder_mt(&stream, &options);
}

DecodeError failureOf(const lzma_ret result, const std::filesystem::path& file)
{
    switch (result) {
    case LZMA_FORMAT_ERROR:
        return NotAnXzFile{ file };
    case LZMA_BUF_ERROR:
        return TruncatedData{ file };
    case LZMA_MEM_ERROR:
        return OutOfMemory{ file };
    default:
        return CorruptData{ file };
    }
}

}  // anonymous namespace

std::expected<void, DecodeError> decodeFile(const std::filesystem::path& file, const std::function<bool(std::string_view)>& consume)
{
    const int descriptor = ::open(file.c_str(), O_RDONLY | O_CLOEXEC);
    if (descriptor < 0) {
        return std::unexpected(CannotReadFile{ file, { errno, std::generic_category() } });
    }
    const OpenFile input{ descriptor };

    Decoder decoder;
    lzma_stream& stream = decoder.stream;
    if (const lzma_ret started = startDecoder(stream); started != LZMA_OK) {
        return std::unexpected(failureOf(started, file));
    }

    std::vector<std::uint8_t> encoded(BufferSize);
    std::vector<std::uint8_t> decoded(BufferSize);
    stream.next_out = decoded.data();
    stream.avail_out = decoded.size();
    lzma_action action = LZMA_RUN;

    while (true) {
        if (stream.avail_in == 0 && action == LZMA_RUN) {
            const ssize_t received = readRetrying(input.descriptor(), encoded);
            if (received < 0) {
                return std::unexpected(CannotReadFile{ file, { errno, std::generic_category() } });
            }
            stream.next_in = encoded.data();
            stream.avail_in = static_cast<std::size_t>(received);
            if (received == 0) {
                action = LZMA_FINISH;
            }
        }

        const lzma_ret result = lzma_code(&stream, action);

        if (stream.avail_out == 0 || result == LZMA_STREAM_END) {
            const std::size_t produced = decoded.size() - stream.avail_out;
            if (produced > 0 && ! consume(std::string_view{ reinterpret_cast<const char*>(decoded.data()), produced })) {
                return std::unexpected(ConsumerStopped{});
            }
            stream.next_out = decoded.data();
            stream.avail_out = decoded.size();
        }

        if (result == LZMA_STREAM_END) {
            return {};
        }
        if (result != LZMA_OK) {
            return std::unexpected(failureOf(result, file));
        }
    }
}

}  // namespace scrap::Xz
