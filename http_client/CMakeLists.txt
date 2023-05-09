cmake_minimum_required(VERSION 3.10.0)

project(http_client)

set(CMAKE_CXX_FLAGS "--std=c++11")

find_package(OpenSSL REQUIRED)

if(OPENSSL_FOUND)
    include_directories(${OPENSSL_INCLUDE_DIRS})
    message(STATUS "OpenSSL Found!")
endif()

find_package(CURL REQUIRED)

if(CURL_FOUND)
    include_directories(${CURL_INCLUDE_DIR})
    message(STATUS "Curl Found: ${CURL_VERSION_STRING} ${CURL_LIBRARIES} ${CURL_LINK_LIBRARIES}!")
else(CURL_FOUND)
    message(FATAL_ERROR "Could not find the CURL library and development files.")
endif()

add_executable(
    http_client
    http_client.cpp
    http_cookie.cpp
    thread_pool.cpp
    thread.cpp)

target_link_libraries(http_client ${CURL_LIBRARIES} pthread OpenSSL::Crypto OpenSSL::SSL)
