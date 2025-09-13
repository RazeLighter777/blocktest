#include "perlinnoiseoverlay.h"

uint8_t PerlinNoiseOverlay::thicknessAt(uint8_t x, uint8_t z) const {
    // Use 2D perlin noise over x,z to determine terrain height
    // noise2D_01 returns [0,1].
    const double n = noise_ ? noise_->noise2D_01(x * frequency_, z * frequency_) : 0.0;
    
    // Convert noise to terrain height
    const uint8_t terrainHeight = static_cast<uint8_t>(baseThickness_ + (n > threshold_ ? maxExtra_ : 0));
    
    // Safety cap so we never exceed reasonable terrain height
    return std::min(terrainHeight, static_cast<uint8_t>(128));
}

void PerlinNoiseOverlay::generateInto(const ChunkSpan& out, const Block* parent) const {
    Block* base = out.data;
    
    // Calculate world coordinates for this chunk
    int32_t worldX = out.position.x * CHUNK_WIDTH;
    int32_t worldY = out.position.y * CHUNK_HEIGHT;
    int32_t worldZ = out.position.z * CHUNK_DEPTH;
    
    for (std::uint32_t z = 0; z < CHUNK_DEPTH; ++z) {
        for (std::uint32_t x = 0; x < CHUNK_WIDTH; ++x) {
            // Calculate world coordinates for this column
            double worldColumnX = worldX + x;
            double worldColumnZ = worldZ + z;
            
            // Get terrain height for this column
            const uint8_t terrainHeight = thicknessAt(static_cast<uint8_t>(worldColumnX), static_cast<uint8_t>(worldColumnZ));
            
            for (std::uint32_t y = 0; y < CHUNK_HEIGHT; ++y) {
                // Calculate world Y coordinate for this block
                int32_t worldBlockY = worldY + y;
                
                const std::size_t idx = static_cast<std::size_t>(z) * out.strideZ + static_cast<std::size_t>(y) * out.strideY + x;
                
                // Place block if we're below the terrain surface
                if (worldBlockY < terrainHeight) {
                    base[idx] = Block::Bedrock;
                } else {
                    base[idx] = parent ? parent[idx] : Block::Empty;
                }
            }
        }
    }
}
