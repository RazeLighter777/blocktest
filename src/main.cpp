#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <memory>
#include <vector>
#include <cstdio>
#include "gl_includes.h"
#include "block.h"
#include "chunkoverlay.h"
#include "position.h"
#include "emptychunk.h"
#include "texture_loader.h"
#include "shader.h"
#include "camera.h"
#include "block_renderer.h"
#include "chunk_mesh.h"
#include "world.h"
#include "chunk_generators.h"

// Window dimensions
const unsigned int WINDOW_WIDTH = 800;
const unsigned int WINDOW_HEIGHT = 600;

// Camera
Camera camera(glm::vec3(0.0f, 5.0f, 5.0f));
float lastX = WINDOW_WIDTH / 2.0f;
float lastY = WINDOW_HEIGHT / 2.0f;
bool firstMouse = true;

// Timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Mouse callback
void mouse_callback([[maybe_unused]] GLFWwindow* window, double xpos, double ypos) {
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    camera.processMouseMovement(xoffset, yoffset);
}

// Scroll callback
void scroll_callback([[maybe_unused]]GLFWwindow* window, [[maybe_unused]] double xoffset, double yoffset) {
    camera.processMouseScroll(yoffset);
}

// Vertex shader source
const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec3 aNormal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec2 TexCoord;
out vec3 Normal;
out vec3 FragPos;

void main() {
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;
    TexCoord = aTexCoord;
    
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
)";

// Fragment shader source
const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

in vec2 TexCoord;
in vec3 Normal;
in vec3 FragPos;

uniform sampler2D atlas;
uniform vec3 lightPos;
uniform vec3 viewPos;

void main() {
    vec4 texColor = texture(atlas, TexCoord);
    if(texColor.a < 0.1)
        discard;
    
    // Ambient lighting
    float ambientStrength = 0.3;
    vec3 ambient = ambientStrength * vec3(1.0, 1.0, 1.0);
    
    // Diffuse lighting
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * vec3(1.0, 1.0, 1.0);
    
    vec3 result = (ambient + diffuse) * texColor.rgb;
    FragColor = vec4(result, texColor.a);
}
)";

int main(void) {
    printf("Block Test Application\n");

    // --- OpenGL/GLFW window setup ---
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return EXIT_FAILURE;
    }
    

    // Set OpenGL version and profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    // Create window (visible for 3D rendering)
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "BlockTest 3D", NULL, NULL);
    if (!window) {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return EXIT_FAILURE;
    }
    glfwMakeContextCurrent(window);

    // --- GLEW setup ---
    glewExperimental = GL_TRUE; // Needed for core profile
    if (glewInit() != GLEW_OK) {
        fprintf(stderr, "Failed to initialize GLEW\n");
        glfwTerminate();
        return EXIT_FAILURE;
    }
    
    // Set mouse callbacks
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    
    // Capture mouse cursor
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // Enable OpenGL features
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // --- Texture loading ---
    printf("Loading texture atlas...\n");
    GLuint atlasTexture = loadTexture("C:/Users/utili/Code/blocktest/assets/atlas.png");
    if (atlasTexture == 0) {
        printf("Failed to load atlas texture\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    } else {
        printf("Successfully loaded atlas texture with ID: %u\n", atlasTexture);
    }

    // --- Initialize rendering ---
    Shader blockShader(vertexShaderSource, fragmentShaderSource);
    BlockRenderer blockRenderer;
    blockRenderer.initialize();
    
    // --- Create World with terrain generation ---
    printf("Creating world with terrain generator...\n");
    auto terrainGenerator = std::make_shared<TerrainChunkGenerator>();
    
    // Set load anchors around the camera starting position
    std::vector<AbsoluteBlockPosition> loadAnchors;
    loadAnchors.push_back(AbsoluteBlockPosition(0, 0, 0));  // Center around origin
    
    // Create world with terrain generator, load radius, and seed
    World world(terrainGenerator, loadAnchors, 3, 42);  // Load 3 chunks in each direction
    
    // Generate chunks around the load anchors
    world.ensureChunksLoaded();
    printf("World chunks loaded.\n");
    
    // Create chunk meshes for rendering
    std::vector<std::unique_ptr<ChunkMesh>> chunkMeshes;
    std::vector<glm::vec3> chunkPositions;
    
    // Build meshes for the loaded chunks
    for (int x = -3; x <= 3; x++) {
        for (int y = -1; y <= 2; y++) {
            for (int z = -3; z <= 3; z++) {
                AbsoluteChunkPosition chunkPos(x, y, z);
                auto chunkOpt = world.chunkAt(chunkPos);
                if (chunkOpt.has_value()) {
                    auto chunk = chunkOpt.value();
                    
                    // Convert chunk data to vector for mesh building
                    std::vector<Block> chunkData(kChunkElemCount);
                    for (size_t i = 0; i < kChunkElemCount; i++) {
                        chunkData[i] = chunk->data[i];
                    }
                    
                    // Create mesh for this chunk
                    auto mesh = std::make_unique<ChunkMesh>();
                    glm::vec3 worldPos(
                        x * CHUNK_WIDTH,
                        y * CHUNK_HEIGHT,
                        z * CHUNK_DEPTH
                    );
                    mesh->buildMesh(chunkData, worldPos);
                    
                    chunkMeshes.push_back(std::move(mesh));
                    chunkPositions.push_back(worldPos);
                }
            }
        }
    }
    
    printf("Built %zu chunk meshes for rendering.\n", chunkMeshes.size());
    
    // Light position
    glm::vec3 lightPos(10.0f, 10.0f, 10.0f);

    printf("Starting render loop...\n");
    
    // Variables for FPS calculation and debug info
    float fpsTimer = 0.0f;
    int frameCount = 0;
    float fps = 0.0f;
    
    // Main render loop
    int64_t lastAnchorX = 0, lastAnchorY = 0, lastAnchorZ = 0;
    while (!glfwWindowShouldClose(window)) {
        // Calculate delta time
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        
        // Update FPS calculation
        frameCount++;
        fpsTimer += deltaTime;
        if (fpsTimer >= 1.0f) {
            fps = frameCount / fpsTimer;
            frameCount = 0;
            fpsTimer = 0.0f;
            
            // Update window title with debug info
            AbsoluteBlockPosition currentPos = toAbsoluteBlock(AbsolutePrecisePosition(camera.position.x, camera.position.y, camera.position.z));
            int loadedChunks = static_cast<int>(chunkMeshes.size());
            
            char titleBuffer[256];
            snprintf(titleBuffer, sizeof(titleBuffer), 
                "BlockTest - Pos: (%.1f, %.1f, %.1f) Block: (%d, %d, %d) Chunks: %d FPS: %.1f",
                camera.position.x, camera.position.y, camera.position.z,
                static_cast<int>(currentPos.x), static_cast<int>(currentPos.y), static_cast<int>(currentPos.z),
                loadedChunks, fps);
            glfwSetWindowTitle(window, titleBuffer);
        }

        // Process input
        camera.processInput(window, deltaTime);

        // Update world anchor to camera's current block position
        AbsoluteBlockPosition anchorBlockPos = toAbsoluteBlock(AbsolutePrecisePosition(camera.position.x, camera.position.y, camera.position.z));
        if (anchorBlockPos.x != lastAnchorX || anchorBlockPos.y != lastAnchorY || anchorBlockPos.z != lastAnchorZ) {
            world = World(terrainGenerator, {anchorBlockPos}, 3, 42); // update anchor
            world.ensureChunksLoaded();

            // Rebuild chunk meshes for new loaded chunks
            chunkMeshes.clear();
            chunkPositions.clear();
            for (int x = -3; x <= 3; x++) {
                for (int y = -1; y <= 2; y++) {
                    for (int z = -3; z <= 3; z++) {
                        AbsoluteChunkPosition chunkPos(x + toAbsoluteChunk(anchorBlockPos).x, y + toAbsoluteChunk(anchorBlockPos).y, z + toAbsoluteChunk(anchorBlockPos).z);
                        auto chunkOpt = world.chunkAt(chunkPos);
                        if (chunkOpt.has_value()) {
                            auto chunk = chunkOpt.value();
                            std::vector<Block> chunkData(kChunkElemCount);
                            for (size_t i = 0; i < kChunkElemCount; i++) {
                                chunkData[i] = chunk->data[i];
                            }
                            auto mesh = std::make_unique<ChunkMesh>();
                            glm::vec3 worldPos(
                                chunkPos.x * CHUNK_WIDTH,
                                chunkPos.y * CHUNK_HEIGHT,
                                chunkPos.z * CHUNK_DEPTH
                            );
                            mesh->buildMesh(chunkData, worldPos);
                            chunkMeshes.push_back(std::move(mesh));
                            chunkPositions.push_back(worldPos);
                        }
                    }
                }
            }
            lastAnchorX = anchorBlockPos.x;
            lastAnchorY = anchorBlockPos.y;
            lastAnchorZ = anchorBlockPos.z;
        }

        // Check for escape key to exit
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        // Clear screen
        glClearColor(0.2f, 0.3f, 0.8f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Use shader
        blockShader.use();

        // Bind texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, atlasTexture);
        blockShader.setInt("atlas", 0);

        // Set matrices
        glm::mat4 model = glm::mat4(1.0f);
        glm::mat4 view = camera.getViewMatrix();
        glm::mat4 projection = camera.getProjectionMatrix(static_cast<float>(WINDOW_WIDTH) / static_cast<float>(WINDOW_HEIGHT));

        blockShader.setMat4("model", model);
        blockShader.setMat4("view", view);
        blockShader.setMat4("projection", projection);

        // Set lighting uniforms
        blockShader.setVec3("lightPos", lightPos);
        blockShader.setVec3("viewPos", camera.position);

        // Render all terrain chunks
        for (size_t i = 0; i < chunkMeshes.size(); i++) {
            chunkMeshes[i]->render();
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    blockRenderer.cleanup();
    for (auto& mesh : chunkMeshes) {
        mesh->cleanup();
    }
    glDeleteTextures(1, &atlasTexture);
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return EXIT_SUCCESS;
}