cmake_minimum_required(VERSION 3.16)

project(LimitOrderBook LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

add_library(order_book
    src/OrderBook.cpp
    src/CommandQueue.cpp
    src/MatchingEngine.cpp
)

target_include_directories(order_book PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(main
    src/main.cpp
)

add_executable(tests
    tests/tests.cpp
)

add_executable(benchmark
    benchmark/benchmark.cpp
)

add_executable(benchmark_threaded
    benchmark/benchmark_threaded.cpp
)

add_executable(benchmark_mpsc
    benchmark/benchmark_mpsc.cpp
)

add_executable(benchmark_mpsc_lockfree
    benchmark/benchmark_mpsc_lockfree.cpp
)

add_executable(benchmark_queue_comparison
    benchmark/benchmark_queue_comparison.cpp
)

target_link_libraries(benchmark_queue_comparison PRIVATE
    order_book
)

target_link_libraries(benchmark_mpsc_lockfree PRIVATE
    order_book
)

target_link_libraries(benchmark_mpsc PRIVATE
    order_book
)

target_link_libraries(benchmark_threaded PRIVATE
    order_book
)

target_link_libraries(main PRIVATE
    order_book
)

target_link_libraries(tests PRIVATE
    order_book
    Threads::Threads
)

target_link_libraries(benchmark PRIVATE
    order_book
)

target_link_libraries(order_book PUBLIC
    Threads::Threads
)