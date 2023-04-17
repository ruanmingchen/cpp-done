# Cmake最低版本要求
cmake_minimum_required(VERSION 3.9.0)

# 设置编译进程名字
set(project_name a.out)

project(${project_name})

# 设置版本号 1.0.1
set(VERSION_MAJOR 1)
set(VERSION_MINOR 0)
set(VERSION_PATCH 1)

# 参数设置
# set(BUILD_ONLY "" CACHE STRING "A semi-colon delimited list of the projects to build")
# set(CPP_STANDARD "11" CACHE STRING "Flag to upgrade the C++ standard used. The default is 11. The minimum is 11.")
# set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Release build by default.")

# 取消第三方库头文件进行依赖检查,liunx加快编译速度
set(CMAKE_DEPENDS_IN_PROJECT_ONLY ON)

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

# 增加宏定义
# add_definitions(-DTEST)
# add_definitions(-DUNICODE)
# add_definitions(-DWIN32_LEAN_AND_MEAN)
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
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_SOURCE_DIR}/bin")
file(GLOB SOURCE_MAIN "${CMAKE_SOURCE_DIR}/*.cpp")
add_executable(${PROJECT_NAME} prohibit_repeated_starts/PidFile.h)
