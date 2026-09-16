cmake_minimum_required(VERSION 3.16)

project(LimitOrderBook LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(order_book
    OrderBook.cpp
)

add_executable(main
    main.cpp
)

add_executable(tests
    tests.cpp
)

target_link_libraries(main PRIVATE order_book)
target_link_libraries(tests PRIVATE order_book)