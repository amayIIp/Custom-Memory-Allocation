#include "arena_allocator.hpp"
#include "pool_allocator.hpp"
#include <iostream>
#include <vector>
#include <cstdlib>

#define TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "Assertion failed: " << #cond << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::abort(); \
        } \
    } while (0)

void test_arena_allocator() {
    std::cout << "Testing ArenaAllocator..." << std::endl;

    constexpr std::size_t buffer_size = 1024;
    std::vector<std::byte> buffer(buffer_size);

    custom_alloc::ArenaAllocator arena(buffer.data(), buffer_size);

    TEST_ASSERT(arena.total_size() == buffer_size);
    TEST_ASSERT(arena.used_bytes() == 0);

    // 1. Basic allocation
    void* p1 = arena.allocate(100, 8);
    TEST_ASSERT(p1 != nullptr);
    TEST_ASSERT(reinterpret_cast<std::uintptr_t>(p1) % 8 == 0);
    TEST_ASSERT(arena.used_bytes() >= 100);

    // 2. Alignment padding allocation
    std::size_t used_before = arena.used_bytes();
    void* p2 = arena.allocate(10, 64);
    TEST_ASSERT(p2 != nullptr);
    TEST_ASSERT(reinterpret_cast<std::uintptr_t>(p2) % 64 == 0);
    TEST_ASSERT(arena.used_bytes() > used_before);

    // 3. Overflow allocation
    void* p3 = arena.allocate(buffer_size, 1);
    TEST_ASSERT(p3 == nullptr); // Should fail

    // 4. Reset test
    arena.reset();
    TEST_ASSERT(arena.used_bytes() == 0);
    void* p4 = arena.allocate(100, 8);
    TEST_ASSERT(p4 != nullptr);
    TEST_ASSERT(arena.used_bytes() == 100);

    // 5. Move construction
    custom_alloc::ArenaAllocator arena_moved = std::move(arena);
    TEST_ASSERT(arena_moved.total_size() == buffer_size);
    TEST_ASSERT(arena_moved.used_bytes() == 100);
    TEST_ASSERT(arena.total_size() == 0);
    TEST_ASSERT(arena.used_bytes() == 0);

    // 6. Move assignment
    custom_alloc::ArenaAllocator arena_assigned(nullptr, 0);
    arena_assigned = std::move(arena_moved);
    TEST_ASSERT(arena_assigned.total_size() == buffer_size);
    TEST_ASSERT(arena_assigned.used_bytes() == 100);

    // 7. Managed buffer construction (1 MB)
    {
        custom_alloc::ArenaAllocator managed_arena(1024 * 1024);
        TEST_ASSERT(managed_arena.total_size() == 1024 * 1024);
        TEST_ASSERT(managed_arena.used_bytes() == 0);
        void* p = managed_arena.allocate(256, 16);
        TEST_ASSERT(p != nullptr);
        TEST_ASSERT(reinterpret_cast<std::uintptr_t>(p) % 16 == 0);
        TEST_ASSERT(managed_arena.used_bytes() == 256);
    }

    // 8. Marker rollback (stack allocator behavior)
    {
        custom_alloc::ArenaAllocator stack_arena(1024);
        void* p1 = stack_arena.allocate(100, 8);
        TEST_ASSERT(p1 != nullptr);
        
        custom_alloc::ArenaAllocator::Marker marker = stack_arena.get_marker();
        std::size_t used_at_marker = stack_arena.used_bytes();
        
        void* p2 = stack_arena.allocate(200, 16);
        TEST_ASSERT(p2 != nullptr);
        TEST_ASSERT(stack_arena.used_bytes() > used_at_marker);
        
        // Rollback
        stack_arena.rollback(marker);
        TEST_ASSERT(stack_arena.used_bytes() == used_at_marker);
        
        // Next allocation should reuse the rolled-back space
        void* p3 = stack_arena.allocate(200, 16);
        TEST_ASSERT(p3 != nullptr);
        TEST_ASSERT(p3 == p2);
    }

    std::cout << "ArenaAllocator tests passed!" << std::endl;
}

void test_pool_allocator() {
    std::cout << "Testing PoolAllocator..." << std::endl;

    constexpr std::size_t buffer_size = 512;
    std::vector<std::byte> buffer(buffer_size);

    // Let's create a pool of 32-byte chunks, 8-byte aligned.
    custom_alloc::PoolAllocator pool(buffer.data(), buffer_size, 32, 8);

    std::size_t total_ch = pool.total_chunks();
    TEST_ASSERT(total_ch > 0);
    TEST_ASSERT(pool.free_chunks() == total_ch);
    TEST_ASSERT(pool.used_chunks() == 0);

    // 1. Allocate all chunks
    std::vector<void*> allocated_ptrs;
    for (std::size_t i = 0; i < total_ch; ++i) {
        void* ptr = pool.allocate(32, 8);
        TEST_ASSERT(ptr != nullptr);
        TEST_ASSERT(reinterpret_cast<std::uintptr_t>(ptr) % 8 == 0);
        allocated_ptrs.push_back(ptr);
    }

    TEST_ASSERT(pool.free_chunks() == 0);
    TEST_ASSERT(pool.used_chunks() == total_ch);

    // 2. Allocate one more (should fail)
    void* p_fail = pool.allocate(32, 8);
    TEST_ASSERT(p_fail == nullptr);

    // 3. Deallocate all chunks
    for (void* ptr : allocated_ptrs) {
        pool.deallocate(ptr, 32);
    }
    TEST_ASSERT(pool.free_chunks() == total_ch);
    TEST_ASSERT(pool.used_chunks() == 0);

    // 4. LIFO check (the last freed block should be the first allocated block next)
    void* last_freed = allocated_ptrs.back();
    void* first_new = pool.allocate(32, 8);
    TEST_ASSERT(first_new == last_freed);
    pool.deallocate(first_new, 32);

    // 5. Reset check
    pool.allocate(32, 8);
    pool.reset();
    TEST_ASSERT(pool.free_chunks() == total_ch);

    // 6. Move operations
    custom_alloc::PoolAllocator pool_moved = std::move(pool);
    TEST_ASSERT(pool_moved.total_chunks() == total_ch);
    TEST_ASSERT(pool.total_chunks() == 0);

    // 7. Growable pool check
    {
        custom_alloc::PoolAllocator growable_pool(5, 32, 8, true);
        TEST_ASSERT(growable_pool.total_chunks() == 5);
        
        std::vector<void*> ptrs;
        for (int i = 0; i < 12; ++i) {
            void* p = growable_pool.allocate(32, 8);
            TEST_ASSERT(p != nullptr);
            ptrs.push_back(p);
        }
        
        TEST_ASSERT(growable_pool.total_chunks() == 15); // Should have grown twice (5 -> 10 -> 15)
        TEST_ASSERT(growable_pool.free_chunks() == 3);
        
        for (void* p : ptrs) {
            growable_pool.deallocate(p, 32);
        }
        TEST_ASSERT(growable_pool.free_chunks() == 15);
    }

    // 8. Randomized Alloc/Free Stress Test (checks free-list corruption)
    {
        custom_alloc::PoolAllocator stress_pool(100, 32, 8, false);
        std::vector<void*> ptrs;
        ptrs.reserve(100);
        
        // Simple LCG random number generator for reproducibility
        std::uint32_t seed = 1337;
        auto next_rand = [&seed]() {
            seed = seed * 1664525u + 1013904223u;
            return seed;
        };

        for (int step = 0; step < 2000; ++step) {
            bool do_alloc = (next_rand() % 2 == 0);
            if (do_alloc && ptrs.size() < 100) {
                void* p = stress_pool.allocate(32, 8);
                TEST_ASSERT(p != nullptr);
                ptrs.push_back(p);
            } else if (!ptrs.empty()) {
                std::size_t index = next_rand() % ptrs.size();
                void* p = ptrs[index];
                ptrs[index] = ptrs.back();
                ptrs.pop_back();
                stress_pool.deallocate(p, 32);
            }
        }
        
        // Clean up remaining allocations
        for (void* p : ptrs) {
            stress_pool.deallocate(p, 32);
        }
        
        TEST_ASSERT(stress_pool.free_chunks() == 100);
        TEST_ASSERT(stress_pool.used_chunks() == 0);
    }

    std::cout << "PoolAllocator tests passed!" << std::endl;
}

int main() {
    test_arena_allocator();
    test_pool_allocator();
    return 0;
}
