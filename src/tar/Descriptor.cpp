#include "tar/Descriptor.h"

#include <cerrno>
#include <system_error>
#include <unistd.h>
#include <utility>

namespace scrap::Tar {

Descriptor::Descriptor(const int value)
    : _value{ value }
{
}

Descriptor::Descriptor(Descriptor&& other) noexcept
    : _value{ std::exchange(other._value, -1) }
{
}

Descriptor& Descriptor::operator=(Descriptor&& other) noexcept
{
    if (this != &other) {
        discard();
        _value = std::exchange(other._value, -1);
    }
    return *this;
}

Descriptor::~Descriptor()
{
    discard();
}

int Descriptor::get() const
{
    return _value;
}

void Descriptor::discard() noexcept
{
    if (_value >= 0) {
        ::close(std::exchange(_value, -1));
    }
}

std::error_code Descriptor::close()
{
    if (_value < 0) {
        return {};
    }
    const int result = ::close(std::exchange(_value, -1));
    return result < 0 ? std::error_code{ errno, std::generic_category() } : std::error_code{};
}

}  // namespace scrap::Tar
