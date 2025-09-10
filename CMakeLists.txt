cmake_minimum_required(VERSION 3.15)
project(blocktest CXX)

find_package(ZLIB REQUIRED)

set(CMAKE_SKIP_RPATH TRUE)

set(CMAKE_CXX_STANDARD 23) # Sets the C++ standard to C++17
set(CMAKE_CXX_STANDARD_REQUIRED ON) # Ensures the specified C++ standard is used

add_executable(${PROJECT_NAME} src/main.cpp)

# Link against ZLIB
target_link_libraries(${PROJECT_NAME} ZLIB::ZLIB)

# Find opengl
find_package(OpenGL REQUIRED)

# Link against OpenGL
target_link_libraries(${PROJECT_NAME} OpenGL::GL)

# Compile options for code correctness
target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)