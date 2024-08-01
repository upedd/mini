#ifndef BITFLAGS_H
#define BITFLAGS_H
#include <bitset>
#include <cstdint>
#include <type_traits>

template <typename T>
class bitflags {
public:
    // Inspiration: https://m-peko.github.io/craft-cpp/posts/different-ways-to-define-binary-flags/

    static_assert(std::is_enum_v<T>, "Flags can only be specialzed for enums.");

    bitflags() = default;

    explicit bitflags(std::uint64_t value) : storage(value) {}

    // turn flag on
    constexpr bitflags& operator+=(const T& rhs) {
        storage.set(static_cast<std::size_t>(rhs));
        return *this; // return the result by reference
    }

    // turn flag off
    constexpr bitflags& operator-=(const T& rhs) {
        storage.reset(static_cast<std::size_t>(rhs));
        return *this; // return the result by reference
    }

    [[nodiscard]] constexpr bool operator[](const T& rhs) const {
        return storage.test(static_cast<std::size_t>(rhs));
    }

    [[nodiscard]] constexpr std::uint64_t to_ullong() const {
        return storage.to_ullong();
    }

private:
    std::bitset<static_cast<std::size_t>(T::size)> storage;
};

#endif //BITFLAGS_H
