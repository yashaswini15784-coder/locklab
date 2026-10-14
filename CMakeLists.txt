cmake_minimum_required(VERSION 3.16)

project(locklab C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

add_library(locklab
    src/stack.c
    src/hazard.c
    src/lf_stack.c
    src/lf_queue.c
    src/ring.c
)

target_include_directories(locklab PUBLIC include)
target_link_libraries(locklab PUBLIC Threads::Threads)

add_executable(test_stack tests/test_stack.c)
target_link_libraries(test_stack PRIVATE locklab)

add_executable(test_lockfree_stack tests/test_lockfree_stack.c)
target_link_libraries(test_lockfree_stack PRIVATE locklab)

add_executable(test_lockfree_queue tests/test_lockfree_queue.c)
target_link_libraries(test_lockfree_queue PRIVATE locklab)

add_executable(aba_demo tests/aba_demo.c)

add_executable(benchmark tests/benchmark.c)
target_link_libraries(benchmark PRIVATE locklab)
add_executable(test_ring tests/test_ring.c)
target_link_libraries(test_ring PRIVATE locklab)
add_executable(locklab_demo tests/demo.c)
target_link_libraries(locklab_demo PRIVATE locklab)
