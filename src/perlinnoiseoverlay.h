#pragma once
#include <memory>
#include "chunkoverlay.h"
#include "perlinnoise.hpp"

// Generates a 2-3 block thick bedrock layer at the bottom of a chunk using Perlin noise.
class PerlinNoiseOverlay : public ChunkOverlay {
public:
    explicit PerlinNoiseOverlay(std::shared_ptr<siv::PerlinNoise> noise,
                                double frequency = 0.07,
                                double threshold = 0.55,
                                uint8_t baseThickness = 10,
                                uint8_t maxExtra = 1,
                                Block blockType = Block::Bedrock)
        : noise_(std::move(noise)),
          frequency_(frequency),
          threshold_(threshold),
          baseThickness_(baseThickness),
          maxExtra_(maxExtra),
          blockType_(blockType) {};

    ~PerlinNoiseOverlay() override = default;

    // Chunk-wide generation. Writes Bedrock at the bottom up to thicknessAt(x,z); otherwise copies parent or Empty.
    void generateInto(const ChunkSpan& out, const Block* parent) const override;

private:
    std::shared_ptr<siv::PerlinNoise> noise_;
    // Noise parameters (tweakable defaults)
    double frequency_ = 0.07;   // spatial frequency for x,z
    double threshold_ = 0.55;   // cutoff to add an extra layer
    uint8_t baseThickness_ = 2; // always at least 2 blocks thick
    uint8_t maxExtra_ = 1;      // can add up to 1 extra layer
    [[maybe_unused]] Block blockType_ = Block::Bedrock;

    uint8_t thicknessAt(uint8_t x, uint8_t z) const;
};
