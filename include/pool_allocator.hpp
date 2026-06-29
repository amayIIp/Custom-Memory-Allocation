#pragma once

#include "allocator_utils.hpp"
#include <cstddef>
#include <cstdint>

namespace custom_alloc {

class PoolAllocator {
public:
    /**
     * @brief Construct a new Pool Allocator using a user-provided memory buffer.
     * @param buffer Pointer to the start of the backing buffer.
     * @param size Size of the backing buffer in bytes.
     * @param chunk_size Size of each block/chunk in bytes (will be aligned/padded if needed).
     * @param chunk_alignment Alignment of each block/chunk in bytes (must be a power of two).
     * @param growable If true, the pool will dynamically allocate additional buffers when exhausted.
     */
    PoolAllocator(void* buffer, std::size_t size, std::size_t chunk_size, std::size_t chunk_alignment, bool growable = false) noexcept;

    /**
     * @brief Construct a new Pool Allocator that manages its own memory buffer.
     * @param num_chunks Initial capacity of the pool in terms of chunks.
     * @param chunk_size Size of each block/chunk in bytes.
     * @param chunk_alignment Alignment of each block/chunk in bytes (must be a power of two).
     * @param growable If true, the pool will dynamically allocate additional buffers when exhausted.
     */
    PoolAllocator(std::size_t num_chunks, std::size_t chunk_size, std::size_t chunk_alignment, bool growable = false) noexcept;

    /**
     * @brief Destructor. Frees any dynamically allocated backing blocks.
     */
    ~PoolAllocator();

    // Disable copy semantics
    PoolAllocator(const PoolAllocator&) = delete;
    PoolAllocator& operator=(const PoolAllocator&) = delete;

    // Enable move semantics
    PoolAllocator(PoolAllocator&& other) noexcept;
    PoolAllocator& operator=(PoolAllocator&& other) noexcept;

    /**
     * @brief Allocates a single chunk of memory.
     * @param size Size of the allocation (must be <= chunk_size).
     * @param alignment Alignment boundary (must be <= chunk_alignment).
     * @return Pointer to the allocated chunk, or nullptr if out of memory/chunks.
     */
    void* allocate(std::size_t size, std::size_t alignment = alignof(std::max_align_t)) noexcept;

    /**
     * @brief Frees a previously allocated chunk, adding it back to the free list.
     * @param ptr Pointer to the chunk to free.
     * @param size Size of the allocation (optional/ignored or used for validation).
     */
    void deallocate(void* ptr, std::size_t size = 0) noexcept;

    /**
     * @brief Resets the pool, rebuilding the free list and invalidating all allocations.
     */
    void reset() noexcept;

    /**
     * @brief Returns the total capacity in terms of number of chunks.
     */
    std::size_t total_chunks() const noexcept { return m_total_chunks; }

    /**
     * @brief Returns the number of chunks currently in use.
     */
    std::size_t used_chunks() const noexcept { return m_total_chunks - m_free_chunks; }

    /**
     * @brief Returns the number of free chunks remaining.
     */
    std::size_t free_chunks() const noexcept { return m_free_chunks; }

#ifdef CUSTOM_ALLOC_DEBUG
    /**
     * @brief Returns the peak memory usage in bytes.
     */
    std::size_t peak_usage() const noexcept { return m_peak_usage; }

    /**
     * @brief Returns the number of live/active allocations.
     */
    std::size_t live_count() const noexcept { return m_live_count; }
#endif

private:
    struct Node {
        Node* next;
    };

    struct BackingBlock {
        void* ptr;
        BackingBlock* next;
    };

    void* m_raw_buffer;
    std::size_t m_raw_size;
    
    std::uintptr_t m_aligned_start;
    std::size_t m_chunk_size;
    std::size_t m_chunk_alignment;
    
    Node* m_free_list;
    std::size_t m_total_chunks;
    std::size_t m_free_chunks;

    bool m_growable;
    std::size_t m_init_num_chunks;
    BackingBlock* m_backing_blocks;

#ifdef CUSTOM_ALLOC_DEBUG
    std::size_t m_peak_usage = 0;
    std::size_t m_live_count = 0;
#endif

    void rebuild_free_list() noexcept;
    bool grow_pool() noexcept;
};

} // namespace custom_alloc
