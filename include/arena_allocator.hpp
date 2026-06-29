#pragma once

#include "allocator_utils.hpp"
#include <cstddef>
#include <cstdint>

namespace custom_alloc {

class ArenaAllocator {
public:
    using Marker = std::uintptr_t;

    /**
     * @brief Construct a new Arena Allocator using a user-provided memory buffer.
     * @param buffer Pointer to the start of the backing buffer.
     * @param size Size of the backing buffer in bytes.
     */
    ArenaAllocator(void* buffer, std::size_t size) noexcept;

    /**
     * @brief Construct a new Arena Allocator that allocates and manages its own buffer.
     * @param capacity The capacity in bytes of the managed buffer.
     */
    explicit ArenaAllocator(std::size_t capacity) noexcept;

    /**
     * @brief Destructor. Frees the internally managed buffer if ownership exists.
     */
    ~ArenaAllocator();

    // Disable copy semantics
    ArenaAllocator(const ArenaAllocator&) = delete;
    ArenaAllocator& operator=(const ArenaAllocator&) = delete;

    // Enable move semantics
    ArenaAllocator(ArenaAllocator&& other) noexcept;
    ArenaAllocator& operator=(ArenaAllocator&& other) noexcept;

    /**
     * @brief Allocates memory of the specified size and alignment.
     * @param size The size of the allocation in bytes.
     * @param alignment The required alignment of the returned address.
     * @return Pointer to the allocated memory, or nullptr if there is insufficient space.
     */
    void* allocate(std::size_t size, std::size_t alignment = alignof(std::max_align_t)) noexcept;

    /**
     * @brief Deallocate is a no-op for ArenaAllocator.
     */
    void deallocate(void* ptr, std::size_t size) noexcept;

    /**
     * @brief Resets the arena bump pointer back to the start.
     *        This invalidates all previously allocated memory.
     */
    void reset() noexcept;

    /**
     * @brief Returns a marker representing the current allocation state.
     */
    Marker get_marker() const noexcept;

    /**
     * @brief Rolls back the arena bump pointer to a previously saved marker.
     *        This invalidates all allocations made after the marker was taken.
     * @param marker The marker to roll back to.
     */
    void rollback(Marker marker) noexcept;

    /**
     * @brief Returns the total size of the backing buffer.
     */
    std::size_t total_size() const noexcept { return m_size; }

    /**
     * @brief Returns the number of bytes currently allocated/used (including alignment padding).
     */
    std::size_t used_bytes() const noexcept;

#ifdef CUSTOM_ALLOC_DEBUG
    /**
     * @brief Returns the peak memory usage tracked during allocations.
     */
    std::size_t peak_usage() const noexcept { return m_peak_usage; }

    /**
     * @brief Returns the number of live/active allocations.
     */
    std::size_t live_count() const noexcept { return m_live_count; }
#endif

private:
    std::uintptr_t m_start;
    std::uintptr_t m_curr;
    std::size_t m_size;
    void* m_owned_buffer = nullptr;

#ifdef CUSTOM_ALLOC_DEBUG
    std::size_t m_peak_usage = 0;
    std::size_t m_live_count = 0;
#endif
};

} // namespace custom_alloc
