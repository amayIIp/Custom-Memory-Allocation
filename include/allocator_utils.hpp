#pragma once

#include <cstdint>
#include <cstddef>
#include <cassert>
#include <new>
#include <utility>

namespace custom_alloc {

/**
 * @brief Checks if a number is a power of two.
 */
inline constexpr bool is_power_of_two(std::size_t x) noexcept {
    return x && ((x & (x - 1)) == 0);
}

/**
 * @brief Aligns a uintptr_t address forward to the specified alignment.
 * @param addr The address to align.
 * @param alignment The alignment boundary (must be a power of two).
 * @return The aligned address.
 */
inline constexpr std::uintptr_t align_forward(std::uintptr_t addr, std::size_t alignment) noexcept {
    assert(is_power_of_two(alignment) && "Alignment must be a power of two");
    return (addr + (alignment - 1)) & ~(alignment - 1);
}

/**
 * @brief Aligns a raw pointer forward to the specified alignment.
 * @param ptr The pointer to align.
 * @param alignment The alignment boundary (must be a power of two).
 * @return The aligned pointer.
 */
inline void* align_forward(void* ptr, std::size_t alignment) noexcept {
    return reinterpret_cast<void*>(align_forward(reinterpret_cast<std::uintptr_t>(ptr), alignment));
}

/**
 * @brief Constructs an object of type T using placement-new on memory allocated by Alloc.
 * @tparam T The type of object to construct.
 * @tparam Allocator The allocator class type.
 * @tparam Args Argument types to pass to the constructor.
 * @param alloc The allocator instance.
 * @param args Arguments to forward to T's constructor.
 * @return Pointer to the constructed object, or nullptr if allocation failed.
 */
template <typename T, typename Allocator, typename... Args>
T* create(Allocator& alloc, Args&&... args) {
    void* ptr = alloc.allocate(sizeof(T), alignof(T));
    if (!ptr) {
        return nullptr;
    }
    return ::new (ptr) T(std::forward<Args>(args)...);
}

/**
 * @brief Destructs an object of type T and deallocates its memory using Alloc.
 * @tparam T The type of object to destroy.
 * @tparam Allocator The allocator class type.
 * @param alloc The allocator instance.
 * @param ptr Pointer to the object to destroy.
 */
template <typename T, typename Allocator>
void destroy(Allocator& alloc, T* ptr) {
    if (ptr) {
        ptr->~T();
        alloc.deallocate(ptr, sizeof(T));
    }
}

} // namespace custom_alloc
