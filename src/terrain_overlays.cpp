#include "terrain_overlays.h"
#include "chunkdims.h"
#include <algorithm>

void TerrainHeightOverlay::generateInto(const ChunkSpan& out, const Block* parent) const {
    // First copy parent data if available, otherwise fill with empty
    if (parent) {
        std::copy(parent, parent + kChunkElemCount, out.data);
    } else {
        std::fill(out.data, out.data + kChunkElemCount, Block::Empty);
    }

    // Calculate world coordinates for this chunk
    int32_t worldX = out.position.x * CHUNK_WIDTH;
    int32_t worldY = out.position.y * CHUNK_HEIGHT;
    int32_t worldZ = out.position.z * CHUNK_DEPTH;

    // Generate terrain for each column
    for (uint32_t z = 0; z < CHUNK_DEPTH; ++z) {
        for (uint32_t x = 0; x < CHUNK_WIDTH; ++x) {
            // Calculate world coordinates for this column
            double worldColumnX = worldX + x;
            double worldColumnZ = worldZ + z;

            // Generate terrain height using Perlin noise
            double terrainHeight = noise_->noise2D(worldColumnX * frequency_, worldColumnZ * frequency_);
            terrainHeight = (terrainHeight + 1.0) * 0.5; // Normalize to 0-1

            // Calculate global surface height for this column
            int32_t globalSurfaceHeight = baseHeight_ + static_cast<int32_t>(terrainHeight * heightVariation_);

            // Fill only up to the global surface height in this chunk
            for (int32_t y = 0; y < static_cast<int32_t>(CHUNK_HEIGHT); ++y) {
                int32_t worldBlockY = worldY + y;
                if (worldBlockY <= globalSurfaceHeight) {
                    const std::size_t idx = static_cast<std::size_t>(z) * out.strideZ + 
                                          static_cast<std::size_t>(y) * out.strideY + x;
                    out.data[idx] = blockType_;
                }
            }
        }
    }
}

void LayerReplaceOverlay::generateInto(const ChunkSpan& out, const Block* parent) const {
    // First copy parent data (this overlay requires a parent)
    if (parent) {
        std::copy(parent, parent + kChunkElemCount, out.data);
    } else {
        // If no parent, fill with empty (though this overlay is meant to modify existing terrain)
        std::fill(out.data, out.data + kChunkElemCount, Block::Empty);
        return;
    }

    // Process each column to find surface and replace layers
    for (uint32_t z = 0; z < CHUNK_DEPTH; ++z) {
        for (uint32_t x = 0; x < CHUNK_WIDTH; ++x) {
            // Find the topmost non-empty block in this column
            int32_t surfaceY = -1;
            for (int32_t y = CHUNK_HEIGHT - 1; y >= 0; --y) {
                const std::size_t idx = static_cast<std::size_t>(z) * out.strideZ + 
                                      static_cast<std::size_t>(y) * out.strideY + x;
                if (out.data[idx] != Block::Empty) {
                    surfaceY = y;
                    break;
                }
            }

            // If we found a surface, replace blocks in the specified range
            if (surfaceY >= 0) {
                int32_t replaceStart = surfaceY - fromTop_;
                int32_t replaceEnd = replaceStart - thickness_ + 1;
                
                for (int32_t y = replaceStart; y >= replaceEnd; --y) {
                    if (y >= 0 && y < static_cast<int32_t>(CHUNK_HEIGHT)) {
                        const std::size_t idx = static_cast<std::size_t>(z) * out.strideZ + 
                                              static_cast<std::size_t>(y) * out.strideY + x;
                        if (out.data[idx] == fromBlock_) {
                            out.data[idx] = toBlock_;
                        }
                    }
                }
            }
        }
    }
}

void SurfaceOverlay::generateInto(const ChunkSpan& out, const Block* parent) const {
    // First copy parent data (this overlay requires a parent)
    if (parent) {
        std::copy(parent, parent + kChunkElemCount, out.data);
    } else {
        // If no parent, just fill with empty
        std::fill(out.data, out.data + kChunkElemCount, Block::Empty);
        return;
    }

    // Process each column to find surface and place block on top
    for (uint32_t z = 0; z < CHUNK_DEPTH; ++z) {
        for (uint32_t x = 0; x < CHUNK_WIDTH; ++x) {
            // Find the topmost non-empty block in this column
            for (int32_t y = CHUNK_HEIGHT - 1; y >= 0; --y) {
                const std::size_t idx = static_cast<std::size_t>(z) * out.strideZ + 
                                      static_cast<std::size_t>(y) * out.strideY + x;
                if (out.data[idx] != Block::Empty) {
                    // Found surface, place block on top if there's space
                    if (y + 1 < static_cast<int32_t>(CHUNK_HEIGHT)) {
                        const std::size_t topIdx = static_cast<std::size_t>(z) * out.strideZ + 
                                                  static_cast<std::size_t>(y + 1) * out.strideY + x;
                        out.data[topIdx] = blockType_;
                    }
                    break;
                }
            }
        }
    }
}