#include "arena_allocator.hpp"
#include <cstdlib>

namespace custom_alloc {

ArenaAllocator::ArenaAllocator(void* buffer, std::size_t size) noexcept
    : m_start(reinterpret_cast<std::uintptr_t>(buffer)),
      m_curr(m_start),
      m_size(size),
      m_owned_buffer(nullptr) {}

ArenaAllocator::ArenaAllocator(std::size_t capacity) noexcept
    : m_start(0),
      m_curr(0),
      m_size(0),
      m_owned_buffer(nullptr) {
    if (capacity > 0) {
        constexpr std::size_t max_align = 64;
        m_owned_buffer = std::malloc(capacity + max_align);
        if (m_owned_buffer) {
            m_start = align_forward(reinterpret_cast<std::uintptr_t>(m_owned_buffer), max_align);
            m_curr = m_start;
            m_size = capacity;
        }
    }
}

ArenaAllocator::~ArenaAllocator() {
    if (m_owned_buffer) {
        std::free(m_owned_buffer);
    }
}

ArenaAllocator::ArenaAllocator(ArenaAllocator&& other) noexcept
    : m_start(other.m_start),
      m_curr(other.m_curr),
      m_size(other.m_size),
      m_owned_buffer(other.m_owned_buffer) {
#ifdef CUSTOM_ALLOC_DEBUG
    m_peak_usage = other.m_peak_usage;
    m_live_count = other.m_live_count;
    other.m_peak_usage = 0;
    other.m_live_count = 0;
#endif
    other.m_start = 0;
    other.m_curr = 0;
    other.m_size = 0;
    other.m_owned_buffer = nullptr;
}

ArenaAllocator& ArenaAllocator::operator=(ArenaAllocator&& other) noexcept {
    if (this != &other) {
        if (m_owned_buffer) {
            std::free(m_owned_buffer);
        }
        m_start = other.m_start;
        m_curr = other.m_curr;
        m_size = other.m_size;
        m_owned_buffer = other.m_owned_buffer;
#ifdef CUSTOM_ALLOC_DEBUG
        m_peak_usage = other.m_peak_usage;
        m_live_count = other.m_live_count;
        other.m_peak_usage = 0;
        other.m_live_count = 0;
#endif
        
        other.m_start = 0;
        other.m_curr = 0;
        other.m_size = 0;
        other.m_owned_buffer = nullptr;
    }
    return *this;
}

void* ArenaAllocator::allocate(std::size_t size, std::size_t alignment) noexcept {
    if (size == 0) [[unlikely]] {
        return nullptr;
    }
    
    std::size_t actual_size = size;
#ifdef CUSTOM_ALLOC_DEBUG
    actual_size += sizeof(std::uint32_t);
#endif

    std::uintptr_t curr_aligned = align_forward(m_curr, alignment);
    
    // Overflow or bounds checking
    if (curr_aligned < m_curr || 
        curr_aligned + actual_size < curr_aligned || 
        curr_aligned + actual_size > m_start + m_size) [[unlikely]] {
        return nullptr;
    }
    
#ifdef CUSTOM_ALLOC_DEBUG
    std::uint32_t* canary = reinterpret_cast<std::uint32_t*>(curr_aligned + size);
    *canary = 0xDEADC0DE;
    m_live_count++;
#endif

    m_curr = curr_aligned + actual_size;

#ifdef CUSTOM_ALLOC_DEBUG
    std::size_t current_used = m_curr - m_start;
    if (current_used > m_peak_usage) {
        m_peak_usage = current_used;
    }
#endif

    return reinterpret_cast<void*>(curr_aligned);
}

void ArenaAllocator::deallocate(void* ptr, std::size_t size) noexcept {
    (void)ptr;
    (void)size;
#ifdef CUSTOM_ALLOC_DEBUG
    if (ptr && size > 0) {
        std::uint32_t* canary = reinterpret_cast<std::uint32_t*>(reinterpret_cast<std::uintptr_t>(ptr) + size);
        assert(*canary == 0xDEADC0DE && "Buffer overflow/canary corruption detected in ArenaAllocator!");
        if (m_live_count > 0) {
            m_live_count--;
        }
    }
#endif
}

void ArenaAllocator::reset() noexcept {
    m_curr = m_start;
#ifdef CUSTOM_ALLOC_DEBUG
    m_live_count = 0;
#endif
}

ArenaAllocator::Marker ArenaAllocator::get_marker() const noexcept {
    return m_curr;
}

void ArenaAllocator::rollback(Marker marker) noexcept {
    assert(marker >= m_start && marker <= m_curr && "Invalid marker rollback target");
    m_curr = marker;
}

std::size_t ArenaAllocator::used_bytes() const noexcept {
    return m_curr - m_start;
}

} // namespace custom_alloc
