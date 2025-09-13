#pragma once

#include <vector>
#include "block.h"
#include <glm/glm.hpp>
#include "gl_includes.h"
struct Vertex {
    glm::vec3 position;
    glm::vec2 texCoord;
    glm::vec3 normal;
};

class BlockRenderer {
public:
    BlockRenderer();
    ~BlockRenderer();
    
    void initialize();
    void renderBlock(Block blockType, const glm::vec3& position);
    void cleanup();
    
    // Geometry generation functions
    static std::vector<Vertex> generateCubeVertices(Block blockType);
    static std::vector<unsigned int> generateCubeIndices();
    static glm::vec2 getTextureUV(Block block, int face, int vertex);
    
private:
    GLuint VAO, VBO, EBO;
    std::vector<Vertex> cubeVertices;
    std::vector<unsigned int> cubeIndices;
    
    void setupCubeGeometry();
    void updateVertexData(Block blockType, const glm::vec3& position);
};