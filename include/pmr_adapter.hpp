#pragma once

#include <memory_resource>
#include <cstddef>
#include <new>

namespace custom_alloc {

/**
 * @brief PMRAdapter wraps a custom allocator to conform to std::pmr::memory_resource.
 *        This allows using the custom allocator directly with standard library PMR containers.
 * @tparam Allocator The custom allocator class (e.g. ArenaAllocator, PoolAllocator).
 */
template <typename Allocator>
class PMRAdapter : public std::pmr::memory_resource {
public:
    /**
     * @brief Construct a PMR Adapter wrapping a reference to a custom allocator.
     * @param alloc The custom allocator instance to wrap.
     */
    explicit PMRAdapter(Allocator& alloc) noexcept : m_alloc(alloc) {}
    ~PMRAdapter() override = default;

    PMRAdapter(const PMRAdapter&) = delete;
    PMRAdapter& operator=(const PMRAdapter&) = delete;

protected:
    /**
     * @brief Allocate memory using the custom allocator.
     */
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
        void* ptr = m_alloc.allocate(bytes, alignment);
        if (!ptr) [[unlikely]] {
            throw std::bad_alloc();
        }
        return ptr;
    }

    /**
     * @brief Deallocate memory using the custom allocator.
     */
    void do_deallocate(void* ptr, std::size_t bytes, std::size_t alignment) override {
        (void)alignment;
        m_alloc.deallocate(ptr, bytes);
    }

    /**
     * @brief Compare this memory resource to another.
     *        They are only equal if they wrap the same underlying adapter instance.
     */
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }

private:
    Allocator& m_alloc;
};

} // namespace custom_alloc
