#include "arena_allocator.hpp"
#include "pool_allocator.hpp"
#include <iostream>
#include <chrono>
#include <vector>
#include <iomanip>
#include <cstdlib>
#include <numeric>

// Helper to prevent compilers from optimizing away variables and operations
template <typename T>
void do_not_optimize(T* ptr) {
#if defined(__GNUC__) || defined(__clang__)
    asm volatile("" : : "g"(ptr) : "memory");
#else
    volatile T* p = ptr;
    (void)p;
#endif
}

struct BenchmarkResult {
    double total_ms;
    double ns_per_op;
    double throughput; // ops per second
};

// Tree node for Scenario 3
struct TreeNode {
    int key;
    int value;
    TreeNode* left;
    TreeNode* right;
};

// Simple BST insertion function
TreeNode* insert_node(TreeNode* root, int key, int value, auto& allocate_fn) {
    if (!root) {
        TreeNode* node = reinterpret_cast<TreeNode*>(allocate_fn(sizeof(TreeNode), alignof(TreeNode)));
        if (!node) return nullptr;
        node->key = key;
        node->value = value;
        node->left = nullptr;
        node->right = nullptr;
        return node;
    }
    if (key < root->key) {
        root->left = insert_node(root->left, key, value, allocate_fn);
    } else {
        root->right = insert_node(root->right, key, value, allocate_fn);
    }
    return root;
}

// Tree traversal to sum values
int sum_tree(TreeNode* root) {
    if (!root) return 0;
    return root->value + sum_tree(root->left) + sum_tree(root->right);
}

// Manual deallocation helper for tree
void free_tree(TreeNode* root, auto& deallocate_fn) {
    if (!root) return;
    free_tree(root->left, deallocate_fn);
    free_tree(root->right, deallocate_fn);
    deallocate_fn(root, sizeof(TreeNode));
}

// LCG random generator for tree keys
std::uint32_t get_next_rand(std::uint32_t& seed) {
    seed = seed * 1664525u + 1013904223u;
    return seed;
}

// Output helper
void print_result(const std::string& name, const BenchmarkResult& res, const BenchmarkResult& baseline) {
    std::cout << std::left << std::setw(20) << name 
              << std::right << std::setw(15) << std::fixed << std::setprecision(3) << res.total_ms 
              << std::right << std::setw(15) << std::fixed << std::setprecision(1) << res.ns_per_op 
              << std::right << std::setw(18) << std::scientific << std::setprecision(2) << res.throughput 
              << std::right << std::setw(12) << std::fixed << std::setprecision(1) << (baseline.ns_per_op / res.ns_per_op) << "x" << "\n";
}

// ==========================================
// SCENARIO 1: Many small fixed-size allocations
// ==========================================
constexpr int S1_ALLOCATIONS = 200'000;
constexpr std::size_t S1_SIZE = 32;
constexpr std::size_t S1_ALIGN = 8;
constexpr int ITERATIONS = 10;

BenchmarkResult s1_std_malloc() {
    double total_ns = 0.0;
    std::vector<void*> ptrs(S1_ALLOCATIONS);

    for (int iter = 0; iter < ITERATIONS; ++iter) {
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < S1_ALLOCATIONS; ++i) {
            ptrs[i] = std::malloc(S1_SIZE);
            do_not_optimize(ptrs[i]);
        }
        for (int i = 0; i < S1_ALLOCATIONS; ++i) {
            std::free(ptrs[i]);
        }
        auto end = std::chrono::high_resolution_clock::now();
        total_ns += std::chrono::duration<double, std::nano>(end - start).count();
    }
    double avg_ns = total_ns / ITERATIONS;
    return {avg_ns / 1'000'000.0, avg_ns / S1_ALLOCATIONS, (S1_ALLOCATIONS * 1'000'000'000.0) / avg_ns};
}

BenchmarkResult s1_pool_alloc() {
    double total_ns = 0.0;
    custom_alloc::PoolAllocator pool(S1_ALLOCATIONS, S1_SIZE, S1_ALIGN, false);
    std::vector<void*> ptrs(S1_ALLOCATIONS);

    for (int iter = 0; iter < ITERATIONS; ++iter) {
        pool.reset();
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < S1_ALLOCATIONS; ++i) {
            ptrs[i] = pool.allocate(S1_SIZE, S1_ALIGN);
            do_not_optimize(ptrs[i]);
        }
        for (int i = S1_ALLOCATIONS - 1; i >= 0; --i) {
            pool.deallocate(ptrs[i], S1_SIZE);
        }
        auto end = std::chrono::high_resolution_clock::now();
        total_ns += std::chrono::duration<double, std::nano>(end - start).count();
    }
    double avg_ns = total_ns / ITERATIONS;
    return {avg_ns / 1'000'000.0, avg_ns / S1_ALLOCATIONS, (S1_ALLOCATIONS * 1'000'000'000.0) / avg_ns};
}

// ==========================================
// SCENARIO 2: Allocate-then-free-all bulk pattern
// ==========================================
constexpr int S2_ALLOCATIONS = 200'000;
constexpr std::size_t S2_SIZE = 32;
constexpr std::size_t S2_ALIGN = 8;

BenchmarkResult s2_std_malloc() {
    return s1_std_malloc(); // Same allocate-then-free pattern for malloc
}

BenchmarkResult s2_arena_alloc() {
    double total_ns = 0.0;
    std::size_t buffer_size = (S2_SIZE + S2_ALIGN) * S2_ALLOCATIONS + 1024;
    custom_alloc::ArenaAllocator arena(buffer_size);
    std::vector<void*> ptrs(S2_ALLOCATIONS);

    for (int iter = 0; iter < ITERATIONS; ++iter) {
        arena.reset();
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < S2_ALLOCATIONS; ++i) {
            ptrs[i] = arena.allocate(S2_SIZE, S2_ALIGN);
            do_not_optimize(ptrs[i]);
        }
        // Bulk deallocation (no-op deallocate, followed by reset outside timing or reset as part of free)
        arena.reset(); 
        auto end = std::chrono::high_resolution_clock::now();
        total_ns += std::chrono::duration<double, std::nano>(end - start).count();
    }
    double avg_ns = total_ns / ITERATIONS;
    return {avg_ns / 1'000'000.0, avg_ns / S2_ALLOCATIONS, (S2_ALLOCATIONS * 1'000'000'000.0) / avg_ns};
}

// ==========================================
// SCENARIO 3: Realistic Workload (BST Tree of 10,000 nodes)
// ==========================================
constexpr int S3_NODES = 10'000;

BenchmarkResult s3_std_malloc() {
    double total_ns = 0.0;
    auto alloc_fn = [](std::size_t size, std::size_t) { return std::malloc(size); };
    auto dealloc_fn = [](void* ptr, std::size_t) { std::free(ptr); };

    for (int iter = 0; iter < ITERATIONS; ++iter) {
        std::uint32_t seed = 42 + iter;
        auto start = std::chrono::high_resolution_clock::now();
        
        TreeNode* root = nullptr;
        for (int i = 0; i < S3_NODES; ++i) {
            root = insert_node(root, get_next_rand(seed) % 100'000, i, alloc_fn);
        }
        
        [[maybe_unused]] volatile int sum = sum_tree(root);
        free_tree(root, dealloc_fn);
        
        auto end = std::chrono::high_resolution_clock::now();
        total_ns += std::chrono::duration<double, std::nano>(end - start).count();
    }
    double avg_ns = total_ns / ITERATIONS;
    return {avg_ns / 1'000'000.0, avg_ns / S3_NODES, (S3_NODES * 1'000'000'000.0) / avg_ns};
}

BenchmarkResult s3_pool_alloc() {
    double total_ns = 0.0;
    custom_alloc::PoolAllocator pool(S3_NODES, sizeof(TreeNode), alignof(TreeNode), false);
    
    auto alloc_fn = [&pool](std::size_t size, std::size_t align) { return pool.allocate(size, align); };
    auto dealloc_fn = [&pool](void* ptr, std::size_t size) { pool.deallocate(ptr, size); };

    for (int iter = 0; iter < ITERATIONS; ++iter) {
        pool.reset();
        std::uint32_t seed = 42 + iter;
        auto start = std::chrono::high_resolution_clock::now();
        
        TreeNode* root = nullptr;
        for (int i = 0; i < S3_NODES; ++i) {
            root = insert_node(root, get_next_rand(seed) % 100'000, i, alloc_fn);
        }
        
        [[maybe_unused]] volatile int sum = sum_tree(root);
        free_tree(root, dealloc_fn);
        
        auto end = std::chrono::high_resolution_clock::now();
        total_ns += std::chrono::duration<double, std::nano>(end - start).count();
    }
    double avg_ns = total_ns / ITERATIONS;
    return {avg_ns / 1'000'000.0, avg_ns / S3_NODES, (S3_NODES * 1'000'000'000.0) / avg_ns};
}

BenchmarkResult s3_arena_alloc() {
    double total_ns = 0.0;
    custom_alloc::ArenaAllocator arena(S3_NODES * sizeof(TreeNode) + 1024);
    
    auto alloc_fn = [&arena](std::size_t size, std::size_t align) { return arena.allocate(size, align); };

    for (int iter = 0; iter < ITERATIONS; ++iter) {
        arena.reset();
        std::uint32_t seed = 42 + iter;
        auto start = std::chrono::high_resolution_clock::now();
        
        TreeNode* root = nullptr;
        for (int i = 0; i < S3_NODES; ++i) {
            root = insert_node(root, get_next_rand(seed) % 100'000, i, alloc_fn);
        }
        
        [[maybe_unused]] volatile int sum = sum_tree(root);
        // Deallocation is a no-op! We just reset the arena.
        arena.reset();
        
        auto end = std::chrono::high_resolution_clock::now();
        total_ns += std::chrono::duration<double, std::nano>(end - start).count();
    }
    double avg_ns = total_ns / ITERATIONS;
    return {avg_ns / 1'000'000.0, avg_ns / S3_NODES, (S3_NODES * 1'000'000'000.0) / avg_ns};
}

int main() {
    std::cout << "========================================================================\n";
    std::cout << "Custom Memory Allocator Benchmarking Suite\n";
    std::cout << "========================================================================\n\n";

    // -------------------------------------------------------------------------
    std::cout << "Scenario 1: Many small fixed-size allocations (" << S1_ALLOCATIONS << " blocks of " << S1_SIZE << "B)\n";
    std::cout << "------------------------------------------------------------------------\n";
    std::cout << std::left << std::setw(20) << "Allocator" 
              << std::right << std::setw(15) << "Total Time (ms)" 
              << std::right << std::setw(15) << "Time/Op (ns)" 
              << std::right << std::setw(18) << "Throughput (ops/s)" 
              << std::right << std::setw(12) << "Speedup" << "\n";
    std::cout << "------------------------------------------------------------------------\n";
    auto s1_baseline = s1_std_malloc();
    print_result("std::malloc", s1_baseline, s1_baseline);
    print_result("PoolAllocator", s1_pool_alloc(), s1_baseline);
    std::cout << "========================================================================\n\n";

    // -------------------------------------------------------------------------
    std::cout << "Scenario 2: Allocate-then-free-all bulk pattern (" << S2_ALLOCATIONS << " blocks of " << S2_SIZE << "B)\n";
    std::cout << "------------------------------------------------------------------------\n";
    std::cout << std::left << std::setw(20) << "Allocator" 
              << std::right << std::setw(15) << "Total Time (ms)" 
              << std::right << std::setw(15) << "Time/Op (ns)" 
              << std::right << std::setw(18) << "Throughput (ops/s)" 
              << std::right << std::setw(12) << "Speedup" << "\n";
    std::cout << "------------------------------------------------------------------------\n";
    auto s2_baseline = s2_std_malloc();
    print_result("std::malloc", s2_baseline, s2_baseline);
    print_result("ArenaAllocator", s2_arena_alloc(), s2_baseline);
    std::cout << "========================================================================\n\n";

    // -------------------------------------------------------------------------
    std::cout << "Scenario 3: Realistic Workload (BST Tree of " << S3_NODES << " nodes)\n";
    std::cout << "------------------------------------------------------------------------\n";
    std::cout << std::left << std::setw(20) << "Allocator" 
              << std::right << std::setw(15) << "Total Time (ms)" 
              << std::right << std::setw(15) << "Time/Op (ns)" 
              << std::right << std::setw(18) << "Throughput (ops/s)" 
              << std::right << std::setw(12) << "Speedup" << "\n";
    std::cout << "------------------------------------------------------------------------\n";
    auto s3_baseline = s3_std_malloc();
    print_result("std::malloc", s3_baseline, s3_baseline);
    print_result("PoolAllocator", s3_pool_alloc(), s3_baseline);
    print_result("ArenaAllocator", s3_arena_alloc(), s3_baseline);
    std::cout << "========================================================================\n";

    return 0;
}
