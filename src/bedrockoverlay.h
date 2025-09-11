#pragma once
#include <memory>
#include "chunkoverlay.h"
#include "perlinnoise.hpp"

// Generates a 2-3 block thick bedrock layer at the bottom of a chunk using Perlin noise.
class BedrockOverlay : public ChunkOverlay {
public:
    explicit BedrockOverlay(std::shared_ptr<siv::PerlinNoise> noise)
        : noise_(std::move(noise)) {}

    Block getBlock(const ChunkLocalPosition pos) const override;
    ~BedrockOverlay() override = default;

private:
    std::shared_ptr<siv::PerlinNoise> noise_;
    // Noise parameters (tweakable defaults)
    double frequency_ = 0.07;   // spatial frequency for x,z
    double threshold_ = 0.55;   // cutoff to add an extra layer
    uint8_t baseThickness_ = 2; // always at least 2 blocks thick
    uint8_t maxExtra_ = 1;      // can add up to 1 extra layer

    uint8_t thicknessAt(uint8_t x, uint8_t z) const;
};
