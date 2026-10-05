cmake_minimum_required(VERSION 3.16)

project(LimitOrderBook LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

add_library(order_book
    src/OrderBook.cpp
    src/CommandQueue.cpp
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