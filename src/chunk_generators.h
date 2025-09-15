#pragma once
#include "world.h"
#include "perlinnoise.hpp"
// Empty chunk generator - generates empty chunks
class FlatworldChunkGenerator : public IWorldgenStrategy, public std::enable_shared_from_this<FlatworldChunkGenerator> {
public:
    FlatworldChunkGenerator(size_t height = 1, Block fillBlock = Block::Grass)
        : height_(height), fillBlock_(fillBlock) {}
    std::shared_ptr<ChunkTransform> generateChunk(const AbsoluteChunkPosition& pos, size_t seed) const override;
private:
    size_t height_;
    Block fillBlock_;
};

/**
 * @brief Generates realistic terrain using Perlin noise. Has three biomes: plains, hills, and mountains.
 * Uses multiple octaves of Perlin noise to create heightmaps and biome maps.
 * Biome transitions are smoothed to avoid abrupt changes.
 */
// class RealisticTerrainChunkGenerator : public IWorldgenStrategy, public std::enable_shared_from_this<RealisticTerrainChunkGenerator> {
// public:

//     RealisticTerrainChunkGenerator(size_t maxHeight = 1024)
//         : maxHeight_(maxHeight), perlinNoise_(std::make_shared<siv::PerlinNoise>(std::random_device{}())) {};
//     std::shared_ptr<ChunkTransform> generateChunk(const AbsoluteChunkPosition& pos, size_t seed) const override;
// private:
//     size_t maxHeight_;
//     std::shared_ptr<siv::PerlinNoise> perlinNoise_;
// };
