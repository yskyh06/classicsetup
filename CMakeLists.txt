cmake_minimum_required(VERSION 3.16)

project(classicsetup LANGUAGES C)

add_executable(classicsetup src/main.c)

set_target_properties(classicsetup PROPERTIES
    C_STANDARD 17
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
)

target_include_directories(classicsetup PRIVATE include)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(classicsetup PRIVATE -Wall -Wextra -Wpedantic)
endif()
