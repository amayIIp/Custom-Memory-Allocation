#include "pool_allocator.hpp"
#include <algorithm>
#include <cstdlib>
#include <cassert>

namespace custom_alloc {

PoolAllocator::PoolAllocator(void* buffer, std::size_t size, std::size_t chunk_size, std::size_t chunk_alignment, bool growable) noexcept
    : m_raw_buffer(buffer),
      m_raw_size(size),
      m_aligned_start(0),
      m_chunk_size(0),
      m_chunk_alignment(chunk_alignment),
      m_free_list(nullptr),
      m_total_chunks(0),
      m_free_chunks(0),
      m_growable(growable),
      m_init_num_chunks(0),
      m_backing_blocks(nullptr) {
          
    assert(is_power_of_two(chunk_alignment) && "Chunk alignment must be a power of two");
    
    if (buffer != nullptr && size > 0) {
        m_aligned_start = align_forward(reinterpret_cast<std::uintptr_t>(buffer), chunk_alignment);
        std::uintptr_t raw_end = reinterpret_cast<std::uintptr_t>(buffer) + size;
        
        if (m_aligned_start < raw_end) {
            std::size_t remaining_size = raw_end - m_aligned_start;
            std::size_t min_chunk = std::max(chunk_size, sizeof(Node));
            m_chunk_size = align_forward(min_chunk, chunk_alignment);
            m_total_chunks = remaining_size / m_chunk_size;
            m_init_num_chunks = m_total_chunks;
            rebuild_free_list();
        }
    }
}

PoolAllocator::PoolAllocator(std::size_t num_chunks, std::size_t chunk_size, std::size_t chunk_alignment, bool growable) noexcept
    : m_raw_buffer(nullptr),
      m_raw_size(0),
      m_aligned_start(0),
      m_chunk_size(0),
      m_chunk_alignment(chunk_alignment),
      m_free_list(nullptr),
      m_total_chunks(0),
      m_free_chunks(0),
      m_growable(growable),
      m_init_num_chunks(num_chunks),
      m_backing_blocks(nullptr) {
          
    assert(is_power_of_two(chunk_alignment) && "Chunk alignment must be a power of two");
    
    if (num_chunks > 0) {
        std::size_t min_chunk = std::max(chunk_size, sizeof(Node));
        m_chunk_size = align_forward(min_chunk, chunk_alignment);
        
        std::size_t required_size = num_chunks * m_chunk_size + chunk_alignment;
        void* buffer = std::malloc(required_size);
        if (buffer) {
            BackingBlock* block = static_cast<BackingBlock*>(std::malloc(sizeof(BackingBlock)));
            if (block) {
                block->ptr = buffer;
                block->next = nullptr;
                m_backing_blocks = block;
                m_total_chunks = num_chunks;
                rebuild_free_list();
            } else {
                std::free(buffer);
            }
        }
    }
}

PoolAllocator::~PoolAllocator() {
    BackingBlock* curr = m_backing_blocks;
    while (curr != nullptr) {
        BackingBlock* next = curr->next;
        std::free(curr->ptr);
        std::free(curr);
        curr = next;
    }
}

PoolAllocator::PoolAllocator(PoolAllocator&& other) noexcept
    : m_raw_buffer(other.m_raw_buffer),
      m_raw_size(other.m_raw_size),
      m_aligned_start(other.m_aligned_start),
      m_chunk_size(other.m_chunk_size),
      m_chunk_alignment(other.m_chunk_alignment),
      m_free_list(other.m_free_list),
      m_total_chunks(other.m_total_chunks),
      m_free_chunks(other.m_free_chunks),
      m_growable(other.m_growable),
      m_init_num_chunks(other.m_init_num_chunks),
      m_backing_blocks(other.m_backing_blocks) {
#ifdef CUSTOM_ALLOC_DEBUG
    m_peak_usage = other.m_peak_usage;
    m_live_count = other.m_live_count;
    other.m_peak_usage = 0;
    other.m_live_count = 0;
#endif
    other.m_raw_buffer = nullptr;
    other.m_raw_size = 0;
    other.m_aligned_start = 0;
    other.m_chunk_size = 0;
    other.m_chunk_alignment = 0;
    other.m_free_list = nullptr;
    other.m_total_chunks = 0;
    other.m_free_chunks = 0;
    other.m_growable = false;
    other.m_init_num_chunks = 0;
    other.m_backing_blocks = nullptr;
}

PoolAllocator& PoolAllocator::operator=(PoolAllocator&& other) noexcept {
    if (this != &other) {
        BackingBlock* curr = m_backing_blocks;
        while (curr != nullptr) {
            BackingBlock* next = curr->next;
            std::free(curr->ptr);
            std::free(curr);
            curr = next;
        }

        m_raw_buffer = other.m_raw_buffer;
        m_raw_size = other.m_raw_size;
        m_aligned_start = other.m_aligned_start;
        m_chunk_size = other.m_chunk_size;
        m_chunk_alignment = other.m_chunk_alignment;
        m_free_list = other.m_free_list;
        m_total_chunks = other.m_total_chunks;
        m_free_chunks = other.m_free_chunks;
        m_growable = other.m_growable;
        m_init_num_chunks = other.m_init_num_chunks;
        m_backing_blocks = other.m_backing_blocks;
#ifdef CUSTOM_ALLOC_DEBUG
        m_peak_usage = other.m_peak_usage;
        m_live_count = other.m_live_count;
        other.m_peak_usage = 0;
        other.m_live_count = 0;
#endif
        
        other.m_raw_buffer = nullptr;
        other.m_raw_size = 0;
        other.m_aligned_start = 0;
        other.m_chunk_size = 0;
        other.m_chunk_alignment = 0;
        other.m_free_list = nullptr;
        other.m_total_chunks = 0;
        other.m_free_chunks = 0;
        other.m_growable = false;
        other.m_init_num_chunks = 0;
        other.m_backing_blocks = nullptr;
    }
    return *this;
}

void* PoolAllocator::allocate(std::size_t size, std::size_t alignment) noexcept {
    if (size > m_chunk_size || alignment > m_chunk_alignment) [[unlikely]] {
        return nullptr;
    }

#ifdef CUSTOM_ALLOC_DEBUG
    bool can_write_canary = (size + sizeof(std::uint32_t) <= m_chunk_size);
#endif

    if (m_free_list == nullptr) [[unlikely]] {
        if (!m_growable || !grow_pool()) {
            return nullptr;
        }
    }

    Node* node = m_free_list;
    m_free_list = m_free_list->next;
    --m_free_chunks;

#ifdef CUSTOM_ALLOC_DEBUG
    if (can_write_canary) {
        std::uint32_t* canary = reinterpret_cast<std::uint32_t*>(reinterpret_cast<std::uintptr_t>(node) + size);
        *canary = 0xDEADC0DE;
    }
    m_live_count++;
    std::size_t current_used_bytes = (m_total_chunks - m_free_chunks) * m_chunk_size;
    if (current_used_bytes > m_peak_usage) {
        m_peak_usage = current_used_bytes;
    }
#endif

    return reinterpret_cast<void*>(node);
}

void PoolAllocator::deallocate(void* ptr, [[maybe_unused]] std::size_t size) noexcept {
    if (ptr == nullptr) [[unlikely]] {
        return;
    }

    [[maybe_unused]] std::uintptr_t addr = reinterpret_cast<std::uintptr_t>(ptr);

#ifndef NDEBUG
    bool is_valid_pointer = false;
    
    if (m_raw_buffer) {
        std::uintptr_t raw_start = reinterpret_cast<std::uintptr_t>(m_raw_buffer);
        if (addr >= raw_start && addr < raw_start + m_raw_size) {
            is_valid_pointer = true;
        }
    }
    
    BackingBlock* curr = m_backing_blocks;
    while (curr != nullptr && !is_valid_pointer) {
        std::uintptr_t block_start = reinterpret_cast<std::uintptr_t>(curr->ptr);
        std::size_t block_size = m_init_num_chunks * m_chunk_size + m_chunk_alignment;
        if (addr >= block_start && addr < block_start + block_size) {
            is_valid_pointer = true;
        }
        curr = curr->next;
    }
    
    assert(is_valid_pointer && "Deallocated pointer does not belong to this PoolAllocator");
    assert((addr % m_chunk_alignment) == 0 && "Deallocated pointer is not aligned properly");
#endif

#ifdef CUSTOM_ALLOC_DEBUG
    if (size > 0 && (size + sizeof(std::uint32_t) <= m_chunk_size)) {
        std::uint32_t* canary = reinterpret_cast<std::uint32_t*>(addr + size);
        assert(*canary == 0xDEADC0DE && "Buffer overflow/canary corruption detected in PoolAllocator!");
    }
    if (m_live_count > 0) {
        m_live_count--;
    }
#endif

    Node* node = reinterpret_cast<Node*>(ptr);
    node->next = m_free_list;
    m_free_list = node;
    ++m_free_chunks;
}

void PoolAllocator::reset() noexcept {
    rebuild_free_list();
#ifdef CUSTOM_ALLOC_DEBUG
    m_live_count = 0;
#endif
}

void PoolAllocator::rebuild_free_list() noexcept {
    m_free_list = nullptr;
    m_free_chunks = 0;

    if (m_raw_buffer && m_aligned_start) {
        std::uintptr_t raw_end = reinterpret_cast<std::uintptr_t>(m_raw_buffer) + m_raw_size;
        if (m_aligned_start < raw_end) {
            std::size_t remaining_size = raw_end - m_aligned_start;
            std::size_t num_chunks = remaining_size / m_chunk_size;
            
            Node* prev = nullptr;
            for (std::size_t i = 0; i < num_chunks; ++i) {
                std::uintptr_t chunk_addr = m_aligned_start + i * m_chunk_size;
                Node* node = reinterpret_cast<Node*>(chunk_addr);
                node->next = prev;
                prev = node;
            }
            m_free_list = prev;
            m_free_chunks = num_chunks;
        }
    }

    BackingBlock* curr = m_backing_blocks;
    while (curr != nullptr) {
        std::uintptr_t aligned_start = align_forward(reinterpret_cast<std::uintptr_t>(curr->ptr), m_chunk_alignment);
        Node* prev = m_free_list;
        for (std::size_t i = 0; i < m_init_num_chunks; ++i) {
            std::uintptr_t chunk_addr = aligned_start + i * m_chunk_size;
            Node* node = reinterpret_cast<Node*>(chunk_addr);
            node->next = prev;
            prev = node;
        }
        m_free_list = prev;
        m_free_chunks += m_init_num_chunks;
        curr = curr->next;
    }
}

bool PoolAllocator::grow_pool() noexcept {
    std::size_t required_size = m_init_num_chunks * m_chunk_size + m_chunk_alignment;
    void* buffer = std::malloc(required_size);
    if (!buffer) {
        return false;
    }

    BackingBlock* block = static_cast<BackingBlock*>(std::malloc(sizeof(BackingBlock)));
    if (!block) {
        std::free(buffer);
        return false;
    }
    block->ptr = buffer;
    block->next = m_backing_blocks;
    m_backing_blocks = block;

    std::uintptr_t aligned_start = align_forward(reinterpret_cast<std::uintptr_t>(buffer), m_chunk_alignment);
    
    Node* prev = m_free_list;
    for (std::size_t i = 0; i < m_init_num_chunks; ++i) {
        std::uintptr_t chunk_addr = aligned_start + i * m_chunk_size;
        Node* node = reinterpret_cast<Node*>(chunk_addr);
        node->next = prev;
        prev = node;
    }
    m_free_list = prev;
    m_total_chunks += m_init_num_chunks;
    m_free_chunks += m_init_num_chunks;
    
    return true;
}

} // namespace custom_alloc
