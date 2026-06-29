#include "allocator_utils.hpp"
#include <iostream>
#include <cstdlib>

#define TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "Assertion failed: " << #cond << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::abort(); \
        } \
    } while (0)

void test_align_forward() {
    // Compile-time checks
    static_assert(custom_alloc::is_power_of_two(1), "1 is a power of 2");
    static_assert(custom_alloc::is_power_of_two(2), "2 is a power of 2");
    static_assert(custom_alloc::is_power_of_two(4), "4 is a power of 2");
    static_assert(custom_alloc::is_power_of_two(8), "8 is a power of 2");
    static_assert(custom_alloc::is_power_of_two(1024), "1024 is a power of 2");
    static_assert(!custom_alloc::is_power_of_two(0), "0 is not a power of 2");
    static_assert(!custom_alloc::is_power_of_two(3), "3 is not a power of 2");
    static_assert(!custom_alloc::is_power_of_two(5), "5 is not a power of 2");
    static_assert(!custom_alloc::is_power_of_two(100), "100 is not a power of 2");
    
    static_assert(custom_alloc::align_forward(std::uintptr_t(0), 8) == 0, "0 aligned to 8 is 0");
    static_assert(custom_alloc::align_forward(1, 8) == 8, "1 aligned to 8 is 8");
    static_assert(custom_alloc::align_forward(7, 8) == 8, "7 aligned to 8 is 8");
    static_assert(custom_alloc::align_forward(8, 8) == 8, "8 aligned to 8 is 8");
    static_assert(custom_alloc::align_forward(9, 8) == 16, "9 aligned to 8 is 16");
    static_assert(custom_alloc::align_forward(15, 16) == 16, "15 aligned to 16 is 16");
    static_assert(custom_alloc::align_forward(16, 16) == 16, "16 aligned to 16 is 16");
    static_assert(custom_alloc::align_forward(17, 16) == 32, "17 aligned to 16 is 32");

    // Runtime checks
    void* ptr0 = reinterpret_cast<void*>(0);
    TEST_ASSERT(custom_alloc::align_forward(ptr0, 8) == reinterpret_cast<void*>(0));

    void* ptr1 = reinterpret_cast<void*>(1);
    TEST_ASSERT(custom_alloc::align_forward(ptr1, 8) == reinterpret_cast<void*>(8));

    void* ptr7 = reinterpret_cast<void*>(7);
    TEST_ASSERT(custom_alloc::align_forward(ptr7, 8) == reinterpret_cast<void*>(8));

    void* ptr8 = reinterpret_cast<void*>(8);
    TEST_ASSERT(custom_alloc::align_forward(ptr8, 8) == reinterpret_cast<void*>(8));

    void* ptr9 = reinterpret_cast<void*>(9);
    TEST_ASSERT(custom_alloc::align_forward(ptr9, 8) == reinterpret_cast<void*>(16));

    void* ptr15 = reinterpret_cast<void*>(15);
    TEST_ASSERT(custom_alloc::align_forward(ptr15, 16) == reinterpret_cast<void*>(16));

    std::cout << "align_forward tests passed successfully!" << std::endl;
}

int main() {
    test_align_forward();
    return 0;
}
