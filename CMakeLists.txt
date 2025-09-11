cmake_minimum_required(VERSION 3.15)
project(blocktest CXX)

set(CMAKE_SKIP_RPATH TRUE)

set(CMAKE_CXX_STANDARD 23) # Sets the C++ standard to C++23
set(CMAKE_CXX_STANDARD_REQUIRED TRUE) # Ensures the specified C++ standard is used

add_executable(${PROJECT_NAME} src/main.cpp src/emptychunk.cpp src/statefulchunkoverlay.cpp src/bedrockoverlay.cpp)
add_executable(tests_${PROJECT_NAME} src/tests.cpp src/emptychunk.cpp src/statefulchunkoverlay.cpp src/bedrockoverlay.cpp)

# Find opengl
find_package(OpenGL REQUIRED)

# Link against OpenGL
target_link_libraries(${PROJECT_NAME} OpenGL::GL)

# Find GLFW
find_package(glfw3 REQUIRED)

# Link against GLFW
target_link_libraries(${PROJECT_NAME} glfw)

# Compile options for code correctness
target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)