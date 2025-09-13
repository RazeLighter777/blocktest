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

std::unique_ptr<ChunkSpan> TerrainChunkGenerator::generateChunk(const AbsoluteChunkPosition& pos, size_t seed) {
    auto chunk = std::make_unique<GeneratedChunkSpan>(pos);
    
    // Create noise generators
    auto bedrockNoise = std::make_shared<siv::PerlinNoise>(seed);
    auto terrainNoise = std::make_shared<siv::PerlinNoise>(seed + 1);
    
    // Create overlay layers from bottom to top
    std::vector<std::shared_ptr<const ChunkOverlay>> layers;
    
    // 1. Bedrock layer at the bottom (1-5 blocks thick)
    auto bedrock = std::make_shared<PerlinNoiseOverlay>(
        bedrockNoise, 
        0.02,     // frequency
        0.4,      // threshold
        1,        // base thickness
        4,        // max extra thickness
        Block::Bedrock
    );
    layers.push_back(bedrock);
    
    // 2. Main stone terrain layer (generates terrain height)
    auto stoneTerrain = std::make_shared<TerrainHeightOverlay>(
        terrainNoise,
        0.01,                    // frequency for terrain variation
        CHUNK_HEIGHT / 2,        // base height (middle of chunk)
        CHUNK_HEIGHT / 4,        // height variation (25% of chunk height)
        Block::Stone
    );
    layers.push_back(stoneTerrain);
    
    // 3. Dirt layer (replace top 2-3 stone blocks with dirt)
    auto dirtLayer = std::make_shared<LayerReplaceOverlay>(
        Block::Stone,   // replace this block type
        Block::Dirt,    // with this block type
        0,              // starting from surface (0 blocks down)
        3               // 3 blocks thick
    );
    layers.push_back(dirtLayer);
    
    // 4. Grass surface layer
    auto grassSurface = std::make_shared<SurfaceOverlay>(Block::Grass);
    layers.push_back(grassSurface);
    
    // Compose all layers and generate the terrain
    auto terrainComposition = compose(layers);
    terrainComposition->generate(*chunk);
    
    return chunk;
}
