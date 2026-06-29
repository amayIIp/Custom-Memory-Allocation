# High-Performance Custom Memory Allocators in C++20

This project provides a robust, production-grade suite of custom memory allocators designed for latency-sensitive applications (such as quantitative trading systems or game engines). 

It implements two primary allocation paradigms:
1. **Arena (Bump) Allocator**: Optimized for bulk-allocation and bulk-deallocation patterns.
2. **Pool (Fixed-Block) Allocator**: Optimized for individual O(1) allocations and deallocations of same-sized objects in arbitrary order.

---

## Project Structure
```text
├── CMakeLists.txt
├── README.md
├── include/
│   ├── allocator_utils.hpp      # Bitmasking alignment helpers, create/destroy lifecycle helpers
│   ├── arena_allocator.hpp      # ArenaAllocator interface
│   ├── pool_allocator.hpp       # PoolAllocator interface
│   └── pmr_adapter.hpp          # std::pmr::memory_resource adapter for STL integration
├── src/
│   ├── arena_allocator.cpp      # ArenaAllocator implementation
│   ├── pool_allocator.cpp       # PoolAllocator implementation
│   └── particle_system_simulation.cpp # Mini-use-case particle system
├── tests/
│   ├── test_align_forward.cpp   # Unit tests for bitwise alignment utility
│   ├── test_allocators.cpp      # Core allocator logic unit tests
│   └── test_cpp_integration.cpp # PMR and C++ integration tests
└── benchmarks/
    └── benchmark_main.cpp       # Automated performance benchmarking suite
```

---

## Design Rationale & Mechanics

### 1. Bitwise Alignment Helper
Every allocator aligns its cursor using bitwise alignment arithmetic:
$$\text{aligned\_addr} = (\text{addr} + \text{alignment} - 1) \ \& \ \sim(\text{alignment} - 1)$$
This executes in a few CPU cycles and relies on alignment boundaries being powers of two.

### 2. Arena (Bump) Allocator
* **Mechanics**: Increments a pointer by the requested size. It contains an optional stack-based rollback extension (`get_marker()` and `rollback(Marker)`) turning it into a fast, temporary stack allocator.
* **Complexity**: $O(1)$ allocation, $O(1)$ bulk deallocation (`reset()`), individual frees are no-ops.
* **Ownership**: Supports wrapping user-provided raw buffers or managing self-allocated heap buffers (aligned to 64-bytes for SIMD/cache-friendliness).

### 3. Pool (Fixed-Block) Allocator
* **Mechanics**: Uses an **intrusive singly-linked free list**. When blocks are unused, their first bytes store the address of the next free chunk (`Node*`). This guarantees zero extra metadata space overhead.
* **Complexity**: $O(1)$ allocation (pop from free list head) and $O(1)$ deallocation (push back to free list head).
* **Exhaustion Policies**:
  * **Strict/Fixed (Default)**: Fails and returns `nullptr` when the pre-allocated chunks are exhausted. This guarantees strict $O(1)$ performance bounds and zero latency jitter.
  * **Growable**: Dynamically allocates a new chunk backing block from the heap via `std::malloc` when empty. This incurs temporal jitter during the growth step but allows safety against out-of-memory errors.

### 4. C++ Integration & PMR Support
* **Lifecycle Helpers**: `create<T>(alloc, args...)` and `destroy<T>(alloc, ptr)` provide placement-new construction and explicit destructor dispatch.
* **PMR Adapter (`pmr_adapter.hpp`)**: Wraps our custom allocators as `std::pmr::memory_resource` targets. This allows modern standard library containers (`std::pmr::vector`, `std::pmr::string`, etc.) to utilize custom arenas/pools natively.
* **Class-Specific Overloads**: Overloading class-specific `operator new` and `operator delete` bypasses global allocator overriding, avoiding recursion risks (e.g. if the allocator's internal structures recursively call global `new`).

### 5. Debug Mode Features (Canaries & Stats)
When compiled under the `CUSTOM_ALLOC_DEBUG` flag (automatically enabled in CMake `Debug` configurations):
* **Sentinel Canaries**: A `0xDEADC0DE` suffix is appended to allocations. On deallocation, this canary is checked; buffer overflows are caught immediately via assert.
* **Telemetry**: Tracks active allocations (`live_count()`) and historical `peak_usage()`.
* **Zero Overhead**: In `Release` mode, these definitions and checks compile away entirely.

---

## Benchmark Results (Release Build, C++20 Clang)

Benchmarks were run for $10$ iterations on a Clang 22.1 compiler targeting Windows MinGW-w64 (UCRT).

### Scenario 1: Many Small Allocations (200,000 blocks of 32B)
*Measures raw throughput of individual allocation and deallocation.*

| Allocator | Total Time (ms) | Time/Op (ns) | Throughput (ops/s) | Speedup |
| :--- | :---: | :---: | :---: | :---: |
| **`std::malloc`** | 16.060 | 80.3 | $1.25 \times 10^7$ | 1.0x (Baseline) |
| **`PoolAllocator`** | 1.493 | 7.5 | $1.34 \times 10^8$ | **10.8x** |

### Scenario 2: Allocate-then-free-all bulk pattern (200,000 blocks of 32B)
*Measures raw performance of high-density bump allocations followed by bulk-free.*

| Allocator | Total Time (ms) | Time/Op (ns) | Throughput (ops/s) | Speedup |
| :--- | :---: | :---: | :---: | :---: |
| **`std::malloc`** | 14.257 | 71.3 | $1.40 \times 10^7$ | 1.0x (Baseline) |
| **`ArenaAllocator`** | 0.400 | 2.0 | $4.99 \times 10^8$ | **35.6x** |

### Scenario 3: Realistic Workload (BST Tree of 10,000 Nodes)
*Building, traversing, and deallocating a Binary Search Tree. Includes non-trivial pointer linkage, search/insertion logic, and key generation.*

| Allocator | Total Time (ms) | Time/Op (ns) | Throughput (ops/s) | Speedup |
| :--- | :---: | :---: | :---: | :---: |
| **`std::malloc`** | 2.295 | 229.5 | $4.36 \times 10^6$ | 1.0x (Baseline) |
| **`PoolAllocator`** | 1.602 | 160.2 | $6.24 \times 10^6$ | **1.4x** |
| **`ArenaAllocator`** | 1.483 | 148.3 | $6.75 \times 10^6$ | **1.5x** |

#### Observation on Scenario 3:
While the microbenchmarks (Scenarios 1 & 2) reflect the raw speed of pointer arithmetic ($10\text{x}$ to $35\text{x}$ faster), Scenario 3 represents a real-world application where code execution is shared with algorithmic traversal and random key generation. Even so, replacing `std::malloc` with `ArenaAllocator` or `PoolAllocator` yields a **$30\text{–}33\%$ reduction in total program runtime** (a **1.4x to 1.5x** speedup).

---

## Practical Application: Particle System Simulation
We implemented an end-to-end particle system in `src/particle_system_simulation.cpp`:
* **`PoolAllocator`**: Handles particle lifecycles. Since particles are spawned and die frequently at arbitrary intervals, the pool recycles dead slots with $O(1)$ efficiency.
* **`ArenaAllocator`**: Serves as a frame-local temporary buffer. It allocates transient collision logs and frame diagnostics during a frame, printing them, and then resets instantly in $O(1)$ at the frame boundary.

---

## Build and Execution

Ensure you have CMake (>= 3.15) and a modern C++20 compiler (Clang/GCC/MSVC).

### 1. Build the Project (Release mode)
```bash
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
mingw32-make # or 'make' on Linux / 'ninja'
```

### 2. Run Tests
```bash
./test_align_forward
./test_allocators
./test_cpp_integration
```

### 3. Run Benchmark and Simulation
```bash
./benchmark_main
./particle_system_simulation
```

### 4. Hardware Profiling (Linux cache misses)
On a Linux platform, you can measure cache-miss reductions using `perf`:
```bash
perf stat -e cache-references,cache-misses,instructions,cycles ./benchmark_main
```
Because the `PoolAllocator` and `ArenaAllocator` pack objects contiguously in cache lines (maximizing temporal and spatial locality), you will observe a dramatic reduction in cache misses compared to the fragmented allocations of the standard system heap allocator.
