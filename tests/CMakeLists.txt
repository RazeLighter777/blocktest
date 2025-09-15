# Find GTest
find_package(GTest REQUIRED)

# Create a library with the source files we want to test
# This excludes main.cpp since tests will have their own main
add_library(blocktest_lib STATIC
    ../src/world.cpp
    ../src/sqlite_chunk_persistence.cpp
    ../src/chunk_generators.cpp
    ../src/texture_loader.cpp
    ../src/block.cpp
    ../src/shader.cpp
    ../src/camera.cpp
    ../src/block_renderer.cpp
    ../src/chunk_mesh.cpp
    ../src/client.cpp
    ../src/server.cpp
    ../src/chunkspan.cpp
    ../src/opengl_loader.cpp
    ../src/position.cpp
    ../src/name_component.cpp
    ../src/player_session.cpp
    ${PROTO_SRCS}
    ${GRPC_SRCS}
)

# Include directories for the library
target_include_directories(blocktest_lib PUBLIC 
    ../src
    ${PROTO_SRC_DIR}
)

# Link the same dependencies as the main executable
target_link_libraries(blocktest_lib PUBLIC
    GLEW::GLEW
    OpenGL::GL
    glfw
    SQLite::SQLite3
    stb::stb
    glm::glm
    gRPC::grpc++
    protobuf::protobuf
    EnTT::EnTT
)

# Apply the same compile options
target_compile_options(blocktest_lib PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
    -Wno-unused-variable
    -Wno-unused-parameter
)

# Test executables
add_executable(test_block test_block.cpp)
target_link_libraries(test_block 
    blocktest_lib
    GTest::gtest 
    GTest::gtest_main
)

add_executable(test_chunkspan test_chunkspan.cpp)
target_link_libraries(test_chunkspan 
    blocktest_lib
    GTest::gtest 
    GTest::gtest_main
)

add_executable(test_position test_position.cpp)
target_link_libraries(test_position 
    blocktest_lib
    GTest::gtest 
    GTest::gtest_main
)

add_executable(test_world test_world.cpp)
target_link_libraries(test_world 
    blocktest_lib
    GTest::gtest 
    GTest::gtest_main
)

add_executable(test_client_server test_client_server.cpp)
target_link_libraries(test_client_server 
    blocktest_lib
    GTest::gtest 
    GTest::gtest_main
)

# Register tests with CTest
add_test(NAME BlockTests COMMAND test_block)
add_test(NAME ChunkSpanTests COMMAND test_chunkspan)
add_test(NAME PositionTests COMMAND test_position)
add_test(NAME WorldTests COMMAND test_world)
add_test(NAME ClientServerTests COMMAND test_client_server)

# Set test properties (longer timeout for integration tests)
set_tests_properties(BlockTests ChunkSpanTests PositionTests WorldTests PROPERTIES TIMEOUT 30)
set_tests_properties(ClientServerTests PROPERTIES TIMEOUT 60)