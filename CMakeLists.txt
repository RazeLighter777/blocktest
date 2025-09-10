cmake_minimum_required(VERSION 3.15)
project(compressor CXX)

find_package(ZLIB REQUIRED)

set(CMAKE_SKIP_RPATH TRUE)

set(CMAKE_CXX_STANDARD 23) # Sets the C++ standard to C++17
set(CMAKE_CXX_STANDARD_REQUIRED ON) # Ensures the specified C++ standard is used

add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME} ZLIB::ZLIB)

# Compile options for code correctness
target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)