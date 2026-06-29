#include "arena_allocator.hpp"
#include "pool_allocator.hpp"
#include "pmr_adapter.hpp"
#include "allocator_utils.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>

#define TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "Assertion failed: " << #cond << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::abort(); \
        } \
    } while (0)

struct TestStruct {
    static int constructor_calls;
    static int destructor_calls;
    
    int val;
    std::string str;

    TestStruct(int v, std::string s) : val(v), str(std::move(s)) {
        constructor_calls++;
    }
    ~TestStruct() {
        destructor_calls++;
    }
};

int TestStruct::constructor_calls = 0;
int TestStruct::destructor_calls = 0;

// Class overloading operator new and delete
class OverloadedWidget {
public:
    int id;
    static int new_calls;
    static int delete_calls;

    OverloadedWidget(int i) : id(i) {}

    static void* operator new(std::size_t size, custom_alloc::ArenaAllocator& alloc) {
        new_calls++;
        return alloc.allocate(size, alignof(OverloadedWidget));
    }

    static void operator delete(void* ptr, custom_alloc::ArenaAllocator& alloc) {
        delete_calls++;
        alloc.deallocate(ptr, sizeof(OverloadedWidget));
    }
    
    static void operator delete(void* ptr) noexcept {
        (void)ptr;
    }
};

int OverloadedWidget::new_calls = 0;
int OverloadedWidget::delete_calls = 0;

void test_lifecycle_helpers() {
    std::cout << "Testing create<T> and destroy<T>..." << std::endl;
    
    custom_alloc::ArenaAllocator arena(1024);
    
    TestStruct::constructor_calls = 0;
    TestStruct::destructor_calls = 0;

    TestStruct* obj = custom_alloc::create<TestStruct>(arena, 42, "hello");
    TEST_ASSERT(obj != nullptr);
    TEST_ASSERT(obj->val == 42);
    TEST_ASSERT(obj->str == "hello");
    TEST_ASSERT(TestStruct::constructor_calls == 1);
    TEST_ASSERT(TestStruct::destructor_calls == 0);

    custom_alloc::destroy(arena, obj);
    TEST_ASSERT(TestStruct::destructor_calls == 1);

    std::cout << "Lifecycle helpers passed!" << std::endl;
}

void test_pmr_integration() {
    std::cout << "Testing PMR adapter with std::pmr::vector..." << std::endl;

    custom_alloc::ArenaAllocator arena(4096);
    custom_alloc::PMRAdapter<custom_alloc::ArenaAllocator> p_res(arena);

    // Create a PMR vector using our memory resource
    std::pmr::vector<std::pmr::string> vec(&p_res);

    vec.push_back("This is a PMR string allocated in our arena");
    vec.push_back("Another one!");
    vec.push_back("And a third one to verify growth!");

    TEST_ASSERT(vec.size() == 3);
    TEST_ASSERT(vec[0] == "This is a PMR string allocated in our arena");
    TEST_ASSERT(vec[1] == "Another one!");
    TEST_ASSERT(vec[2] == "And a third one to verify growth!");

    std::cout << "PMR integration passed!" << std::endl;
}

void test_class_overloads() {
    std::cout << "Testing class-specific operator new/delete..." << std::endl;

    custom_alloc::ArenaAllocator arena(1024);
    
    OverloadedWidget::new_calls = 0;
    OverloadedWidget::delete_calls = 0;

    OverloadedWidget* widget = new (arena) OverloadedWidget(99);
    TEST_ASSERT(widget != nullptr);
    TEST_ASSERT(widget->id == 99);
    TEST_ASSERT(OverloadedWidget::new_calls == 1);

    // Call destructor manually (standard placement new cleanup)
    widget->~OverloadedWidget();
    // And operator delete matching the placement new
    OverloadedWidget::operator delete(widget, arena);
    TEST_ASSERT(OverloadedWidget::delete_calls == 1);

    std::cout << "Class overloads passed!" << std::endl;
}

void test_debug_features() {
#ifdef CUSTOM_ALLOC_DEBUG
    std::cout << "Testing Debug features (Canaries and stats)..." << std::endl;

    // 1. Stats test
    {
        custom_alloc::ArenaAllocator arena(1024);
        TEST_ASSERT(arena.live_count() == 0);
        TEST_ASSERT(arena.peak_usage() == 0);

        void* p1 = arena.allocate(100, 8);
        TEST_ASSERT(arena.live_count() == 1);
        
        void* p2 = arena.allocate(200, 8);
        TEST_ASSERT(arena.live_count() == 2);
        
        arena.deallocate(p1, 100);
        TEST_ASSERT(arena.live_count() == 1);

        arena.deallocate(p2, 200);
        TEST_ASSERT(arena.live_count() == 0);

        TEST_ASSERT(arena.peak_usage() > 0);
    }

    // 2. Pool stats test
    {
        custom_alloc::PoolAllocator pool(10, 32, 8, false);
        TEST_ASSERT(pool.live_count() == 0);

        void* p1 = pool.allocate(32, 8);
        TEST_ASSERT(pool.live_count() == 1);

        pool.deallocate(p1, 32);
        TEST_ASSERT(pool.live_count() == 0);
        TEST_ASSERT(pool.peak_usage() > 0);
    }
    std::cout << "Debug features passed!" << std::endl;
#else
    std::cout << "Debug features omitted (Non-debug build)." << std::endl;
#endif
}

int main() {
    test_lifecycle_helpers();
    test_pmr_integration();
    test_class_overloads();
    test_debug_features();
    return 0;
}
