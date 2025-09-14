#include "chunk_generators.h"
#include "chunkdims.h"
#include <algorithm>

namespace {
// A ChunkSpan that owns its storage so data remains valid while referenced.
struct OwningChunkSpan : public ChunkSpan {
    std::vector<Block> storage;
    explicit OwningChunkSpan(const AbsoluteChunkPosition pos)
        : ChunkSpan(pos), storage(kChunkElemCount, Block::Empty) {
        data = storage.data();
        // Keep default strides from base: strideY=CHUNK_WIDTH, strideZ=CHUNK_WIDTH*CHUNK_HEIGHT
    }
};

// A ChunkSpan that can be generated using overlays
struct GeneratedChunkSpan : public OwningChunkSpan {
    explicit GeneratedChunkSpan(const AbsoluteChunkPosition pos) 
        : OwningChunkSpan(pos) {}
};
}

std::unique_ptr<ChunkSpan> EmptyChunkGenerator::generateChunk(const AbsoluteChunkPosition& pos, size_t /*seed*/) {
    return std::make_unique<GeneratedChunkSpan>(pos);
}

std::unique_ptr<ChunkSpan> TerrainChunkGenerator::generateChunk(const AbsoluteChunkPosition& pos, size_t /*seed*/) {
    auto chunk = std::make_unique<GeneratedChunkSpan>(pos);
    
    // Create flat grass terrain with surface at global y=3
    const int32_t flatSurfaceHeight = 3;  // Surface at global y=3
    
    // For chunks that are entirely above the terrain surface, return empty chunks
    int32_t chunkMinY = pos.y * CHUNK_HEIGHT;
    int32_t chunkMaxY = chunkMinY + CHUNK_HEIGHT - 1;
    if (chunkMinY > flatSurfaceHeight + 1 || chunkMaxY < 0) {
        std::fill(chunk->data, chunk->data + kChunkElemCount, Block::Empty);
        return chunk;
    }
    
    // Calculate world coordinates for this chunk
    int32_t worldY = pos.y * CHUNK_HEIGHT;
    
    // Fill chunk with appropriate blocks based on height
    for (uint32_t z = 0; z < CHUNK_DEPTH; ++z) {
        for (uint32_t y = 0; y < CHUNK_HEIGHT; ++y) {
            for (uint32_t x = 0; x < CHUNK_WIDTH; ++x) {
                int32_t worldBlockY = worldY + y;
                const std::size_t idx = static_cast<std::size_t>(z) * chunk->strideZ + 
                                      static_cast<std::size_t>(y) * chunk->strideY + x;
                
                if (worldBlockY < 0) {
                    // Below ground level: Empty
                    chunk->data[idx] = Block::Empty;
                } else if (worldBlockY == 0) {
                    // Ground level: Bedrock
                    chunk->data[idx] = Block::Bedrock;
                } else if (worldBlockY <= 1) {
                    // Stone layer
                    chunk->data[idx] = Block::Stone;
                } else if (worldBlockY <= 2) {
                    // Dirt layer 
                    chunk->data[idx] = Block::Dirt;
                } else if (worldBlockY == 3) {
                    // Surface: Grass at y=3
                    chunk->data[idx] = Block::Grass;
                } else {
                    // Above surface: Empty
                    chunk->data[idx] = Block::Empty;
                }
            }
        }
    }
    
    return chunk;
}
