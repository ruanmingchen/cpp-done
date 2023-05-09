# Cmake最低版本要求
cmake_minimum_required(VERSION 3.9.0)

set(CMAKE_DEPENDS_IN_PROJECT_ONLY ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_SOURCE_DIR}/build/test_bin")

# 输出编译器的类型
if(CMAKE_CONFIGURATION_TYPES)
    message("--Multi-configuration generator")
else()
    message("----Single-configuration generator")
endif()

# 默认开启Debug
# liunx下是not,注意为空格式
if(NOT CMAKE_BUILD_TYPE OR "${CMAKE_BUILD_TYPE}" MATCHES "None" OR "${CMAKE_BUILD_TYPE}" MATCHES " ")
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build configuration" FORCE)
    message("----CMAKE_BUILD_TYPE FORCE SET Debug")
else()
    # 校验
    if("${CMAKE_BUILD_TYPE}" MATCHES "Release" OR "${CMAKE_BUILD_TYPE}" MATCHES "RelWithDebInfo" OR "${CMAKE_BUILD_TYPE}" MATCHES "MinSizeRel"
        OR "${CMAKE_BUILD_TYPE}" MATCHES "Debug")
        set(CMAKE_BUILD_TYPE "${CMAKE_BUILD_TYPE}")
        message("----set CMAKE_BUILD_TYPE is ${CMAKE_BUILD_TYPE}")
    else()
        message("----CMAKE_BUILD_TYPE is not right")
    endif()
endif()

# 检测linux下c++编译版版本
if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    include(CheckCXXCompilerFlag)

    foreach(cxx_feature_name "c++17" "c++1z" "c++14" "c++1y" "c++11" "c++0x")
        set(cxx_feature_flag "-std=${cxx_feature_name}")
        string(REPLACE "+" "x" cxx_feature_suffix ${cxx_feature_name})
        CHECK_CXX_COMPILER_FLAG(${cxx_feature_flag} COMPILER_SUPPORTS_${cxx_feature_suffix})

        if(COMPILER_SUPPORTS_${cxx_feature_suffix})
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${cxx_feature_flag}")
            message("-- The compiler ${BUILDTOOL_PREFIX}-g++ has ${cxx_feature_name} support")
            break()
        endif()
    endforeach()
endif()

# 设置编译器格式
if(MSVC)
    if(MSVC_VERSION LESS 1900)
        message("--- less vs2015 version")
    endif()

    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3 /O2 /bigobj")

    # was causing conflics with gtest build
    string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG})

    if("${MSVC_RUNTIME_LIBRARY_CONFIG}" STREQUAL "")
        set(MSVC_RUNTIME_LIBRARY_CONFIG "/MT")
    endif()

    foreach(flag_var CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE)
        if("${MSVC_RUNTIME_LIBRARY_CONFIG}" STREQUAL "/MT")
            string(REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
        elseif("${MSVC_RUNTIME_LIBRARY_CONFIG}" STREQUAL "/MD")
            string(REPLACE "/MT" "/MD" ${flag_var} "${${flag_var}}")
        else()
            string(REPLACE "/MD" "${MSVC_RUNTIME_LIBRARY_CONFIG}" ${flag_var} "${${flag_var}}")
            string(REPLACE "/MT" "${MSVC_RUNTIME_LIBRARY_CONFIG}" ${flag_var} "${${flag_var}}")
        endif()
    endforeach()
else()
    # 设置c++编辑器
    set(CMAKAE_CXX_COMPILER g++)

    if(NOT CMAKE_SYSTEM_NAME MATCHES "Linux")
        set(make_mode c++14)
        message("-- set ${make_mode} make mode")
        set(CMAKE_CXX_FLAGS -std=${make_mode})
    endif()

    # 设置C++编辑选项
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -g -W -fpic -fPIC -lssl -lcrypto -ldl -lpthread")

    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")
    endif()

    if(${CMAKE_BUILD_TYPE} MATCHES "Debug")
        message("-- CMAKE_BUILD_TYPE is  ${CMAKE_BUILD_TYPE} mode")

        # 设置C++debug版本编辑选项 --D_DEBUG宏 --ggdb生成debug信息
        set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -DDEBUG -D_DEBUG -ggdb")
    else()
        message("-- CMAKE_BUILD_TYPE is  ${CMAKE_BUILD_TYPE} mode")

        # 设置C++release编辑选项 -02表示优化级别 -g1最小调试信息 -g2全部 -go无 -DNDEBUG屏蔽断言
        set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -g1 -DNDEBUG -DN_DEBUG")
    endif()
endif()

include_directories(${CMAKE_SOURCE_DIR})

add_subdirectory(concurrent_map)
