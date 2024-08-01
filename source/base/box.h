#ifndef BOX_H
#define BOX_H
#include <memory>

// Inspiration: https://www.foonathan.net/2022/05/recursive-variant-box/

namespace bite {
    /**
     * Unique Pointer with value like semantics
     * @tparam T
     */
    template <typename T>
    class box {
        std::unique_ptr<T> ptr;

    public:
        // box() : ptr(nullptr) {}
        box(T&& value) : ptr(new T(std::move(value))) {}
        box(const T& value) : ptr(new T(value)) {}

        box(const box& other) : box(*other.ptr) {}

        box& operator=(const box& other) {
            *ptr = *other.ptr;
            return *this;
        }

        ~box() = default;

        T& operator*() { return *ptr; }
        const T& operator*() const { return *ptr; }

        T* operator->() { return ptr.get(); }
        const T* operator->() const { return ptr.get(); }

        box(box&& other) noexcept = default;
        box& operator=(box&& other) noexcept = default;

        //[[nodiscard]] bool is_empty() const { return ptr == nullptr; }
    };
}
#endif //BOX_H
