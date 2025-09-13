#include "chunk_mesh.h"

ChunkMesh::ChunkMesh() : VAO(0), VBO(0), EBO(0) {}

ChunkMesh::~ChunkMesh() {
    cleanup();
}

void ChunkMesh::buildMesh(const std::vector<Block>& chunkData, const glm::vec3& chunkPosition) {
    vertices.clear();
    indices.clear();
    
    unsigned int vertexOffset = 0;
    
    // Iterate through all blocks in the chunk
    for (int x = 0; x < CHUNK_WIDTH; ++x) {
        for (int y = 0; y < CHUNK_HEIGHT; ++y) {
            for (int z = 0; z < CHUNK_DEPTH; ++z) {
                int blockIndex = getBlockIndex(x, y, z);
                Block blockType = chunkData[blockIndex];
                
                // Skip empty/air blocks
                if (blockType == Block::Empty || blockType == Block::Air) {
                    continue;
                }
                
                glm::vec3 blockPosition = chunkPosition + getBlockPosition(x, y, z);
                
                // Check each face and add if it should be rendered
                for (int face = 0; face < 6; ++face) {
                    if (shouldRenderFace(chunkData, x, y, z, face)) {
                        addBlockFace(blockType, blockPosition, face);
                        
                        // Add indices for this face (2 triangles = 6 indices)
                        indices.push_back(vertexOffset + 0);
                        indices.push_back(vertexOffset + 1);
                        indices.push_back(vertexOffset + 2);
                        indices.push_back(vertexOffset + 2);
                        indices.push_back(vertexOffset + 3);
                        indices.push_back(vertexOffset + 0);
                        
                        vertexOffset += 4;
                    }
                }
            }
        }
    }
    
    setupMesh();
}

void ChunkMesh::render() {
    if (isEmpty()) return;
    
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void ChunkMesh::update(const std::vector<Block>& chunkData, const glm::vec3& chunkPosition) {
    cleanup();
    buildMesh(chunkData, chunkPosition);
}

void ChunkMesh::cleanup() {
    if (VAO != 0) {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO);
        VAO = VBO = EBO = 0;
    }
}

void ChunkMesh::setupMesh() {
    if (vertices.empty()) return;
    
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    
    glBindVertexArray(VAO);
    
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
    
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

bool ChunkMesh::shouldRenderFace(const std::vector<Block>& chunkData, int x, int y, int z, int face) {
    // Face directions: 0=front, 1=back, 2=left, 3=right, 4=top, 5=bottom
    int nx = x, ny = y, nz = z;
    
    switch (face) {
        case 0: nz++; break; // Front
        case 1: nz--; break; // Back
        case 2: nx--; break; // Left
        case 3: nx++; break; // Right
        case 4: ny++; break; // Top
        case 5: ny--; break; // Bottom
    }
    
    // If neighbor is outside chunk bounds, render the face
    if (nx < 0 || nx >= CHUNK_WIDTH || 
        ny < 0 || ny >= CHUNK_HEIGHT || 
        nz < 0 || nz >= CHUNK_DEPTH) {
        return true;
    }
    
    // Check if neighbor block is transparent
    int neighborIndex = getBlockIndex(nx, ny, nz);
    Block neighborBlock = chunkData[neighborIndex];
    
    return (neighborBlock == Block::Empty || neighborBlock == Block::Air);
}

glm::vec3 ChunkMesh::getBlockPosition(int x, int y, int z) const {
    return glm::vec3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
}

int ChunkMesh::getBlockIndex(int x, int y, int z) const {
    // Standard Minecraft-like layout: x + y * CHUNK_WIDTH + z * CHUNK_WIDTH * CHUNK_HEIGHT
    return x + y * CHUNK_WIDTH + z * CHUNK_WIDTH * CHUNK_HEIGHT;
}

void ChunkMesh::addBlockFace(Block blockType, const glm::vec3& position, int face) {
    // Face positions and normals
    glm::vec3 facePositions[6][4] = {
        // Front face (z+)
        {{-0.5f, -0.5f,  0.5f}, { 0.5f, -0.5f,  0.5f}, { 0.5f,  0.5f,  0.5f}, {-0.5f,  0.5f,  0.5f}},
        // Back face (z-)
        {{ 0.5f, -0.5f, -0.5f}, {-0.5f, -0.5f, -0.5f}, {-0.5f,  0.5f, -0.5f}, { 0.5f,  0.5f, -0.5f}},
        // Left face (x-)
        {{-0.5f, -0.5f, -0.5f}, {-0.5f, -0.5f,  0.5f}, {-0.5f,  0.5f,  0.5f}, {-0.5f,  0.5f, -0.5f}},
        // Right face (x+)
        {{ 0.5f, -0.5f,  0.5f}, { 0.5f, -0.5f, -0.5f}, { 0.5f,  0.5f, -0.5f}, { 0.5f,  0.5f,  0.5f}},
        // Top face (y+)
        {{-0.5f,  0.5f,  0.5f}, { 0.5f,  0.5f,  0.5f}, { 0.5f,  0.5f, -0.5f}, {-0.5f,  0.5f, -0.5f}},
        // Bottom face (y-)
        {{-0.5f, -0.5f, -0.5f}, { 0.5f, -0.5f, -0.5f}, { 0.5f, -0.5f,  0.5f}, {-0.5f, -0.5f,  0.5f}}
    };
    
    glm::vec3 faceNormals[6] = {
        { 0.0f,  0.0f,  1.0f}, // Front
        { 0.0f,  0.0f, -1.0f}, // Back
        {-1.0f,  0.0f,  0.0f}, // Left
        { 1.0f,  0.0f,  0.0f}, // Right
        { 0.0f,  1.0f,  0.0f}, // Top
        { 0.0f, -1.0f,  0.0f}  // Bottom
    };
    
    for (int vertex = 0; vertex < 4; ++vertex) {
        Vertex v;
        v.position = position + facePositions[face][vertex];
        v.normal = faceNormals[face];
        v.texCoord = BlockRenderer::getTextureUV(blockType, face, vertex);
        vertices.push_back(v);
    }
}