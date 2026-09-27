cmake_minimum_required(VERSION 3.21)

project(NT2ANDROID LANGUAGES CXX)

option(NT2_BUILD_TESTS "Build Windows research tools" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(nt2core SHARED
    core/src/byte_reader.cpp
    core/src/ntdb_reader.cpp
    core/src/m9p_scanner.cpp
)

target_include_directories(nt2core PUBLIC
    \${CMAKE_CURRENT_SOURCE_DIR}/core/include
)

if (MSVC)
    target_compile_options(nt2core PRIVATE /W4 /permissive-)
else()
    target_compile_options(nt2core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if (WIN32)
    target_compile_definitions(nt2core PRIVATE NT2_PLATFORM_WINDOWS=1)
elseif (ANDROID)
    target_compile_definitions(nt2core PRIVATE NT2_PLATFORM_ANDROID=1)
else()
    target_compile_definitions(nt2core PRIVATE NT2_PLATFORM_GENERIC=1)
endif()

set_target_properties(nt2core PROPERTIES OUTPUT_NAME nt2core)

if (NT2_BUILD_TESTS AND NOT ANDROID)
    add_executable(nt2db_inspect tools/nt2db_inspect.cpp)
    target_link_libraries(nt2db_inspect PRIVATE nt2core)
endif()
