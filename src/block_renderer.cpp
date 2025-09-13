#include "block_renderer.h"

#include <glm/gtc/matrix_transform.hpp>

BlockRenderer::BlockRenderer() : VAO(0), VBO(0), EBO(0) {}

BlockRenderer::~BlockRenderer() {
    cleanup();
}

void BlockRenderer::initialize() {
    setupCubeGeometry();
    
    // Generate OpenGL objects
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    
    // Bind VAO
    glBindVertexArray(VAO);
    
    // Bind and set vertex buffer
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, cubeVertices.size() * sizeof(Vertex), cubeVertices.data(), GL_DYNAMIC_DRAW);
    
    // Bind and set element buffer
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, cubeIndices.size() * sizeof(unsigned int), cubeIndices.data(), GL_STATIC_DRAW);
    
    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
    glEnableVertexAttribArray(0);
    
    // Texture coordinate attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoord));
    glEnableVertexAttribArray(1);
    
    // Normal attribute
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(2);
    
    glBindVertexArray(0);
}

void BlockRenderer::renderBlock(Block blockType, const glm::vec3& position) {
    updateVertexData(blockType, position);
    
    // Update VBO with new vertex data
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, cubeVertices.size() * sizeof(Vertex), cubeVertices.data());
    
    // Render
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(cubeIndices.size()), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void BlockRenderer::cleanup() {
    if (VAO != 0) {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO);
        VAO = VBO = EBO = 0;
    }
}

std::vector<Vertex> BlockRenderer::generateCubeVertices(Block blockType) {
    std::vector<Vertex> vertices;
    
    // Cube positions (front, back, left, right, top, bottom faces)
    glm::vec3 positions[24] = {
        // Front face
        {-0.5f, -0.5f,  0.5f}, { 0.5f, -0.5f,  0.5f}, { 0.5f,  0.5f,  0.5f}, {-0.5f,  0.5f,  0.5f},
        // Back face
        { 0.5f, -0.5f, -0.5f}, {-0.5f, -0.5f, -0.5f}, {-0.5f,  0.5f, -0.5f}, { 0.5f,  0.5f, -0.5f},
        // Left face
        {-0.5f, -0.5f, -0.5f}, {-0.5f, -0.5f,  0.5f}, {-0.5f,  0.5f,  0.5f}, {-0.5f,  0.5f, -0.5f},
        // Right face
        { 0.5f, -0.5f,  0.5f}, { 0.5f, -0.5f, -0.5f}, { 0.5f,  0.5f, -0.5f}, { 0.5f,  0.5f,  0.5f},
        // Top face
        {-0.5f,  0.5f,  0.5f}, { 0.5f,  0.5f,  0.5f}, { 0.5f,  0.5f, -0.5f}, {-0.5f,  0.5f, -0.5f},
        // Bottom face
        {-0.5f, -0.5f, -0.5f}, { 0.5f, -0.5f, -0.5f}, { 0.5f, -0.5f,  0.5f}, {-0.5f, -0.5f,  0.5f}
    };
    
    // Normals for each face
    glm::vec3 normals[6] = {
        { 0.0f,  0.0f,  1.0f}, // Front
        { 0.0f,  0.0f, -1.0f}, // Back
        {-1.0f,  0.0f,  0.0f}, // Left
        { 1.0f,  0.0f,  0.0f}, // Right
        { 0.0f,  1.0f,  0.0f}, // Top
        { 0.0f, -1.0f,  0.0f}  // Bottom
    };
    
    // Texture coordinates for each vertex of each face
    // Note: texCoords array removed as we use getTextureUV function instead
    
    vertices.reserve(24);
    
    for (int face = 0; face < 6; ++face) {
        for (int vertex = 0; vertex < 4; ++vertex) {
            Vertex v;
            v.position = positions[face * 4 + vertex];
            v.normal = normals[face];
            v.texCoord = getTextureUV(blockType, face, vertex);
            vertices.push_back(v);
        }
    }
    
    return vertices;
}

std::vector<unsigned int> BlockRenderer::generateCubeIndices() {
    std::vector<unsigned int> indices;
    indices.reserve(36);
    
    // 6 faces, 2 triangles per face, 3 vertices per triangle
    for (unsigned int face = 0; face < 6; ++face) {
        unsigned int offset = face * 4;
        
        // First triangle
        indices.push_back(offset + 0);
        indices.push_back(offset + 1);
        indices.push_back(offset + 2);
        
        // Second triangle
        indices.push_back(offset + 2);
        indices.push_back(offset + 3);
        indices.push_back(offset + 0);
    }
    
    return indices;
}

glm::vec2 BlockRenderer::getTextureUV(Block block, int /* face */, int vertex) {
    auto [texX, texY] = getTextureIndex(block);
    
    // Convert texture atlas coordinates to UV coordinates
    float u = static_cast<float>(texX) / BLOCKS_PER_ROW;
    float v = static_cast<float>(texY) / BLOCKS_PER_ROW;
    float uvSize = 1.0f / BLOCKS_PER_ROW;
    
    // UV coordinates for each vertex of a face (0=bottom-left, 1=bottom-right, 2=top-right, 3=top-left)
    glm::vec2 uvOffsets[4] = {
        {0.0f, uvSize}, {uvSize, uvSize}, {uvSize, 0.0f}, {0.0f, 0.0f}
    };
    
    return glm::vec2(u, v) + uvOffsets[vertex];
}

void BlockRenderer::setupCubeGeometry() {
    cubeVertices = generateCubeVertices(Block::Stone); // Default block type
    cubeIndices = generateCubeIndices();
}

void BlockRenderer::updateVertexData(Block blockType, const glm::vec3& position) {
    cubeVertices = generateCubeVertices(blockType);
    
    // Translate vertices to position
    for (auto& vertex : cubeVertices) {
        vertex.position += position;
    }
}