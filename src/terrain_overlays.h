#pragma once
#include "chunkoverlay.h"
#include "perlinnoise.hpp"
#include <memory>

// Generates terrain height map and fills blocks up to calculated height with specified block type
class TerrainHeightOverlay : public ChunkOverlay {
public:
    explicit TerrainHeightOverlay(
        std::shared_ptr<siv::PerlinNoise> noise,
        double frequency = 0.01,
        int32_t baseHeight = 32,
        int32_t heightVariation = 16,
        Block blockType = Block::Stone)
        : noise_(std::move(noise)),
          frequency_(frequency),
          baseHeight_(baseHeight),
          heightVariation_(heightVariation),
          blockType_(blockType) {}

    void generateInto(const ChunkSpan& out, const Block* parent) const override;

private:
    std::shared_ptr<siv::PerlinNoise> noise_;
    double frequency_;
    int32_t baseHeight_;
    int32_t heightVariation_;
    Block blockType_;
};

// Replaces blocks of a certain type with another type in a height range
class LayerReplaceOverlay : public ChunkOverlay {
public:
    explicit LayerReplaceOverlay(
        Block fromBlock,
        Block toBlock,
        int32_t fromTop = 0,
        int32_t thickness = 1)
        : fromBlock_(fromBlock),
          toBlock_(toBlock),
          fromTop_(fromTop),
          thickness_(thickness) {}

    void generateInto(const ChunkSpan& out, const Block* parent) const override;

private:
    Block fromBlock_;
    Block toBlock_;
    int32_t fromTop_;     // How many blocks from the top surface to start replacing
    int32_t thickness_;   // How many blocks thick this layer should be
};

// Finds the top surface and places a specific block type on top
class SurfaceOverlay : public ChunkOverlay {
public:
    explicit SurfaceOverlay(Block blockType = Block::Grass)
        : blockType_(blockType) {}

    void generateInto(const ChunkSpan& out, const Block* parent) const override;

private:
    Block blockType_;
};