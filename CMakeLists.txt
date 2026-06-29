cmake_minimum_required(VERSION 3.15)
project(CustomMemoryAllocation VERSION 1.0.0 LANGUAGES CXX)

# Require C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags for warnings and optimizations
if(MSVC)
    add_compile_options(/W4 /WX /Ot /Oy /O2)
else()
    add_compile_options(-Wall -Wextra -Werror -pedantic -O3 -fno-omit-frame-pointer)
endif()

# Core Allocator Library
add_library(custom_allocators
    src/arena_allocator.cpp
    src/pool_allocator.cpp
)

target_include_directories(custom_allocators PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Enable debug features (canaries/statistics) in Debug build type or via manual override
option(CUSTOM_ALLOC_DEBUG_FEATURES "Enable canaries and statistics" OFF)

if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CUSTOM_ALLOC_DEBUG_FEATURES)
    target_compile_definitions(custom_allocators PUBLIC CUSTOM_ALLOC_DEBUG)
    message(STATUS "Custom Allocator Debug features: ENABLED")
else()
    message(STATUS "Custom Allocator Debug features: DISABLED")
endif()

# Test Targets
add_executable(test_align_forward tests/test_align_forward.cpp)
target_link_libraries(test_align_forward PRIVATE custom_allocators)

add_executable(test_allocators tests/test_allocators.cpp)
target_link_libraries(test_allocators PRIVATE custom_allocators)

add_executable(test_cpp_integration tests/test_cpp_integration.cpp)
target_link_libraries(test_cpp_integration PRIVATE custom_allocators)

# Simulation Use-Case Target
add_executable(particle_system_simulation src/particle_system_simulation.cpp)
target_link_libraries(particle_system_simulation PRIVATE custom_allocators)

# Benchmark Targets
add_executable(benchmark_main benchmarks/benchmark_main.cpp)
target_link_libraries(benchmark_main PRIVATE custom_allocators)

# Optional: Add testing integration with CTest
enable_testing()
add_test(NAME TestAlignForward COMMAND test_align_forward)
add_test(NAME TestAllocators COMMAND test_allocators)
add_test(NAME TestCppIntegration COMMAND test_cpp_integration)
