cmake_minimum_required(VERSION 3.10.0)

project(load_library)

set(CMAKE_CXX_FLAGS "--std=c++11")

if(NOT DEFINED BOOST_ROOT)
    set(BOOST_ROOT $ENV{BOOST_ROOT})
endif()

if((${CMAKE_SYSTEM_NAME} STREQUAL "Linux") AND (NOT BOOST_ROOT))
    message(STATUS "setting boost_root for Linux")
    set(BOOST_ROOT "/usr/local")
endif()
message("BOOST_ROOT is ${BOOST_ROOT}")

find_package(Boost 1.70.0 REQUIRED COMPONENTS system)

message("Boost_FOUND is ${Boost_FOUND}")
message("Boost_INCLUDE_DIRS is ${Boost_INCLUDE_DIRS}")
message("Boost_LIBRARIES is ${Boost_LIBRARIES}")
message("Boost_LIB_DIAGNOSTIC_DEFINITIONS is ${Boost_LIB_DIAGNOSTIC_DEFINITIONS}")
message("Boost_DIR is ${Boost_DIR}")
message("Boost_CONFIG is ${Boost_CONFIG}")
message("Boost_VERSION is ${Boost_VERSION}")

include_directories(${Boost_INCLUDE_DIR})

add_executable(
        load_library
        test.cpp
        load_library.h
)

target_link_libraries(load_library dl ${Boost_LIBRARIES})
