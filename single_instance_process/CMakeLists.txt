cmake_minimum_required(VERSION 3.10.0)

project(single_instance_process)

set(CMAKE_CXX_FLAGS "--std=c++11")

add_executable(single_instance_process test.cpp single_instance_process.cpp)
target_link_libraries(single_instance_process pthread)