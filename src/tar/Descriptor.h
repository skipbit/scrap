#pragma once

#include <system_error>

namespace scrap::Tar {

/**
 * @brief Owns a file descriptor and closes it on destruction.
 *
 * A negative value stands for no descriptor.
 */
class Descriptor {
public:
    Descriptor() = default;
    explicit Descriptor(int value);
    Descriptor(Descriptor&& other) noexcept;
    Descriptor& operator=(Descriptor&& other) noexcept;
    Descriptor(const Descriptor&) = delete;
    Descriptor& operator=(const Descriptor&) = delete;
    ~Descriptor();

    [[nodiscard]] int get() const;

    /**
     * @brief Close the descriptor now.
     *
     * @return What the operating system reported, nothing when none was held.
     */
    [[nodiscard]] std::error_code close();

private:
    void discard() noexcept;

    int _value = -1;
};

}  // namespace scrap::Tar
