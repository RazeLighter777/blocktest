#pragma once

#include "world.h"
#include "statefulchunkoverlay.h"
#include "perlinnoiseoverlay.h"
#include "terrain_overlays.h"
#include "emptychunk.h"

// Empty chunk generator - generates empty chunks
class EmptyChunkGenerator : public IChunkGenerator {
public:
    std::unique_ptr<ChunkSpan> generateChunk(const AbsoluteChunkPosition& pos, size_t seed) override;
};

// Terrain generator using Perlin noise
class TerrainChunkGenerator : public IChunkGenerator {
public:
    std::unique_ptr<ChunkSpan> generateChunk(const AbsoluteChunkPosition& pos, size_t seed) override;
};

