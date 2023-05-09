cmake_minimum_required(VERSION 3.10.0)

project(thread_pool)

set(CMAKE_CXX_FLAGS "--std=c++11")

add_executable(thread_pool test.cpp thread_pool.cpp)
target_link_libraries(thread_pool pthread)