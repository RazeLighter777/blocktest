#include "bedrockoverlay.h"

uint8_t BedrockOverlay::thicknessAt(uint8_t x, uint8_t z) const {
    // Use 2D perlin noise over x,z to determine extra thickness [0..maxExtra_]
    // noise2D_01 returns [0,1].
    const double n = noise_ ? noise_->noise2D_01(x * frequency_, z * frequency_) : 0.0;
    const uint8_t extra = (n > threshold_) ? maxExtra_ : 0;
    const uint8_t t = static_cast<uint8_t>(baseThickness_ + extra);
    // Safety cap so we never exceed CHUNK_HEIGHT.
    return (t > CHUNK_HEIGHT ? CHUNK_HEIGHT : t);
}

Block BedrockOverlay::getBlock(const ChunkLocalPosition pos) const {
    // Bedrock occupies the bottom layers: y in [0, thicknessAt(x,z)-1]
    const uint8_t t = thicknessAt(pos.x, pos.z);
    if (pos.y < t) {
        return Block::Bedrock;
    }
    return Block::Empty;
}
