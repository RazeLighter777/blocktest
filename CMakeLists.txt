cmake_minimum_required(VERSION 3.15)
project(blocktest CXX)

set(CMAKE_SKIP_RPATH TRUE)

set(CMAKE_CXX_STANDARD 20) # Sets the C++ standard to C++20
set(CMAKE_CXX_STANDARD_REQUIRED TRUE) # Ensures the specified C++ standard is used

# Enable testing
enable_testing()
include(CTest)

add_executable(${PROJECT_NAME} src/main.cpp src/world.cpp src/sqlite_chunk_persistence.cpp src/chunk_generators.cpp src/texture_loader.cpp src/block.cpp src/shader.cpp src/camera.cpp src/block_renderer.cpp src/chunk_mesh.cpp src/client.cpp src/server.cpp src/chunk_generators.cpp src/chunkspan.cpp src/name_component.cpp src/player_session.cpp)

include_directories()
# find glew
find_package(GLEW REQUIRED)
# link against glew
target_link_libraries(${PROJECT_NAME} PRIVATE GLEW::GLEW)
# Find opengl
find_package(OpenGL REQUIRED)

# Link against OpenGL
target_link_libraries(${PROJECT_NAME} PRIVATE OpenGL::GL)

# Find GLFW
find_package(glfw3 REQUIRED)

# Add GLFW include directories
target_include_directories(${PROJECT_NAME} PRIVATE ${glfw3_INCLUDE_DIRS})

# Link against GLFW
target_link_libraries(${PROJECT_NAME} PRIVATE glfw)

# find sqlite3
find_package(SQLite3 REQUIRED)
# link against sqlite3
target_link_libraries(${PROJECT_NAME} PRIVATE SQLite::SQLite3)

# find stb
find_package(stb REQUIRED CONFIG)
# link against stb
target_link_libraries(${PROJECT_NAME} PRIVATE stb::stb)

# find glm
find_package(glm REQUIRED CONFIG)
# link against glm
target_link_libraries(${PROJECT_NAME} PRIVATE glm::glm)


# find grpc and protobuf
find_package(protobuf REQUIRED)
find_package(gRPC REQUIRED)

# link against grpc
target_link_libraries(${PROJECT_NAME} PRIVATE gRPC::grpc++ protobuf::protobuf)

# find entt
find_package(EnTT REQUIRED)
# link against entt
target_link_libraries(${PROJECT_NAME} PRIVATE EnTT::EnTT)
# Compile options for code correctness
target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
    #disable unused variable warning for now
    -Wno-unused-variable
    -Wno-unused-parameter
)

# Add protobuf compilation
set(PROTO_FILES ${CMAKE_CURRENT_SOURCE_DIR}/proto/blockserver.proto)
set(PROTO_SRC_DIR ${CMAKE_CURRENT_BINARY_DIR}/proto)
file(MAKE_DIRECTORY ${PROTO_SRC_DIR})

set(PROTO_SRCS "${PROTO_SRC_DIR}/blockserver.pb.cc")
set(PROTO_HDRS "${PROTO_SRC_DIR}/blockserver.pb.h")
set(GRPC_SRCS "${PROTO_SRC_DIR}/blockserver.grpc.pb.cc")
set(GRPC_HDRS "${PROTO_SRC_DIR}/blockserver.grpc.pb.h")

add_custom_command(
      OUTPUT "${PROTO_SRCS}" "${PROTO_HDRS}" "${GRPC_SRCS}" "${GRPC_HDRS}"
      COMMAND protobuf::protoc
      ARGS --grpc_out "${PROTO_SRC_DIR}"
           --cpp_out "${PROTO_SRC_DIR}"
           -I "${CMAKE_CURRENT_SOURCE_DIR}/proto"
           --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
           "${PROTO_FILES}"
      DEPENDS "${PROTO_FILES}")

# Include generated protobuf files
target_sources(${PROJECT_NAME} PRIVATE ${PROTO_SRCS} ${GRPC_SRCS})

# Include proto headers directory
target_include_directories(${PROJECT_NAME} PRIVATE ${PROTO_SRC_DIR})

# Add tests subdirectory
add_subdirectory(tests)
