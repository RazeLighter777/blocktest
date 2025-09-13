cmake_minimum_required(VERSION 3.15)
project(blocktest CXX)

set(CMAKE_SKIP_RPATH TRUE)

set(CMAKE_CXX_STANDARD 23) # Sets the C++ standard to C++23
set(CMAKE_CXX_STANDARD_REQUIRED TRUE) # Ensures the specified C++ standard is used

add_executable(${PROJECT_NAME} src/main.cpp src/emptychunk.cpp src/statefulchunkoverlay.cpp src/perlinnoiseoverlay.cpp src/world.cpp src/sqlite_chunk_persistence.cpp src/chunk_generators.cpp src/terrain_overlays.cpp src/texture_loader.cpp src/block.cpp src/shader.cpp src/camera.cpp src/block_renderer.cpp src/chunk_mesh.cpp)
add_executable(tests_${PROJECT_NAME} src/tests.cpp src/emptychunk.cpp src/statefulchunkoverlay.cpp src/perlinnoiseoverlay.cpp src/world.cpp src/sqlite_chunk_persistence.cpp src/chunk_generators.cpp src/terrain_overlays.cpp src/texture_loader.cpp src/block.cpp src/shader.cpp src/camera.cpp src/block_renderer.cpp src/chunk_mesh.cpp)

# find glew
find_package(GLEW REQUIRED)
# link against glew
target_link_libraries(${PROJECT_NAME} PRIVATE GLEW::GLEW)
target_link_libraries(tests_${PROJECT_NAME} PRIVATE GLEW::GLEW)
# Find opengl
find_package(OpenGL REQUIRED)

# Link against OpenGL
target_link_libraries(${PROJECT_NAME} PRIVATE OpenGL::GL)

# Find GLFW
find_package(glfw3 REQUIRED)

# Link against GLFW
target_link_libraries(${PROJECT_NAME} PRIVATE glfw)

# find sqlite3
find_package(SQLite3 REQUIRED)
# link against sqlite3
target_link_libraries(${PROJECT_NAME} PRIVATE SQLite::SQLite3)
target_link_libraries(tests_${PROJECT_NAME} PRIVATE SQLite::SQLite3)

# find stb
find_package(stb REQUIRED CONFIG)
# link against stb
target_link_libraries(${PROJECT_NAME} PRIVATE stb::stb)
target_link_libraries(tests_${PROJECT_NAME} PRIVATE stb::stb)

# find glm
find_package(glm REQUIRED CONFIG)
# link against glm
target_link_libraries(${PROJECT_NAME} PRIVATE glm::glm)
target_link_libraries(tests_${PROJECT_NAME} PRIVATE glm::glm)

# Compile options for code correctness
target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
    -g
)