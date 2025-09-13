#pragma once

#include <glm/glm.hpp>
#include <vector>
#include "block.h"
#include "block_renderer.h"
#include "chunkdims.h"

class ChunkMesh {
public:
    ChunkMesh();
    ~ChunkMesh();
    
    void buildMesh(const std::vector<Block>& chunkData, const glm::vec3& chunkPosition);
    void render();
    void update(const std::vector<Block>& chunkData, const glm::vec3& chunkPosition);
    void cleanup();
    
    bool isEmpty() const { return vertices.empty(); }
    
private:
    GLuint VAO, VBO, EBO;
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    
    void setupMesh();
    bool shouldRenderFace(const std::vector<Block>& chunkData, int x, int y, int z, int face);
    glm::vec3 getBlockPosition(int x, int y, int z) const;
    int getBlockIndex(int x, int y, int z) const;
    void addBlockFace(Block blockType, const glm::vec3& position, int face);
};