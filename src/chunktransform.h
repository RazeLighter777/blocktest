#pragma once
#include <memory>
#include <cstdint>
#include <vector>
#include <algorithm>
#include "block.h"
#include "position.h"
#include "chunkdims.h"
#include "position.h"
#include "chunkspan.h"
#include "perlinnoise.hpp"
#include <functional>


class ChunkTransform;
class CombinedChunkTransform;
class MergeChunkTransform;
class ChunkTransform : public std::enable_shared_from_this<ChunkTransform> {
public:
    virtual ~ChunkTransform() = default;
    virtual void apply(ChunkSpan& chunk) const = 0;
    //operator overload to chain transforms
    /**
     * @brief Combines this transform with another transform, returning a new transform that applies both in sequence.
     * @param other The other transform to combine with.
     * @return A shared pointer to a new CombinedChunkTransform that applies both transforms.
     */
    std::shared_ptr<ChunkTransform> operator+(const std::shared_ptr<ChunkTransform>& other) const {
        // Create a new transform that applies both this and the other transform
        auto self = std::const_pointer_cast<ChunkTransform>(shared_from_this());
        return std::static_pointer_cast<ChunkTransform>(std::make_shared<CombinedChunkTransform>(self, other));
    }
    std::shared_ptr<ChunkTransform> operator|(const std::shared_ptr<ChunkTransform>& other) const {
        // Create a new transform that applies this transform and then the other transform
        auto self = std::const_pointer_cast<ChunkTransform>(shared_from_this());
        return std::static_pointer_cast<ChunkTransform>(std::make_shared<MergeChunkTransform>(self, other));
    }

private:
};


// second generator overwrites the first where they both apply
class CombinedChunkTransform : public ChunkTransform {
public:
    CombinedChunkTransform(std::shared_ptr<ChunkTransform> first, std::shared_ptr<ChunkTransform> second)
        : first_(first), second_(second) {}
    void apply(ChunkSpan& chunk) const override {
        if (first_) first_->apply(chunk);
        if (second_) second_->apply(chunk);
    }

private:
    std::shared_ptr<ChunkTransform> first_;
    std::shared_ptr<ChunkTransform> second_;
};

// second generator only applies where the first one left empty blocks
class MergeChunkTransform : public ChunkTransform {
public:
    MergeChunkTransform(std::shared_ptr<ChunkTransform> first, std::shared_ptr<ChunkTransform> second)
        : first_(first), second_(second) {}
    void apply(ChunkSpan& chunk) const override {
        // copy original storage
        ChunkSpan copiedChunkOne(chunk);
        ChunkSpan copiedChunkTwo(chunk);
        if (first_) first_->apply(copiedChunkOne);
        if (second_) second_->apply(copiedChunkTwo);
        // merge results: where first is empty, take from second
        for (size_t i = 0; i < chunk.storage.size(); ++i) {
            if (copiedChunkOne.storage[i] != Block::Empty) {
                chunk.storage[i] = copiedChunkOne.storage[i];
            } else if (copiedChunkTwo.storage[i] != Block::Empty) {
                chunk.storage[i] = copiedChunkTwo.storage[i];
            }
        }
    }

private:
    std::shared_ptr<ChunkTransform> first_;
    std::shared_ptr<ChunkTransform> second_;
};


class LambdaChunkTransform : public ChunkTransform {
public:
    using TransformFunc = std::function<void(ChunkSpan&)>;
    LambdaChunkTransform(TransformFunc func) : func_(func) {}
    void apply(ChunkSpan& chunk) const override {
        if (func_) func_(chunk);
    }

private:
    const TransformFunc func_;
};

/**
 * @brief A ChunkTransform that fills the chunk with empty blocks.
 */
class EmptyChunkTransform : public ChunkTransform {
public:
    void apply(ChunkSpan& chunk) const override {
        // Set all blocks to empty
        std::fill(chunk.storage.begin(), chunk.storage.end(), Block::Empty);
    }
};

/**
 * @brief A ChunkTransform that does nothing. Useful as a default or placeholder.
 */
class NullChunkTransform : public ChunkTransform {
public:
    void apply(ChunkSpan& chunk) const override {
        // Do nothing
    }
};

/**
 * @brief A ChunkTransform that fills the chunk with a specified block type.
 */
class FillChunkTransform : public ChunkTransform {
public:
    FillChunkTransform(Block block) : block_(block) {}
    void apply(ChunkSpan& chunk) const override {
        // Fill all blocks with the specified block type 
        std::fill(chunk.storage.begin(), chunk.storage.end(), block_);
    }
private:
    const Block block_;
};

/**
 * @brief A ChunkTransform that generates terrain using Perlin noise. Uses the AbsoluteBlockPosition of the chunk to ensure continuity.
 * Takes a shared pointer to a PerlinNoise instance for noise generation.

 * All params mandatory.
 * @param noise The PerlinNoise instance to use for terrain generation.
 * @param scale The scale of the noise (higher values = more zoomed out).
 * @param octaves The number of noise octaves to combine for fractal noise.
 * @param threshold The noise threshold above which blocks are filled (0.0 to 1.0).
 * @param fillBlock The block type to fill when the noise value exceeds the threshold.
 * @param startHeight Mandatory absolute block position height offset to add to the noise value before applying the threshold. Blocks below this height are always filled.
 * @param maxHeight Maximum height (absolute block position) to fill. Mandatory.
 */
class PerlinNoiseChunkTransform : public ChunkTransform {
public:
    PerlinNoiseChunkTransform(std::shared_ptr<siv::PerlinNoise> noise, double scale, int octaves, double threshold, Block fillBlock, int startHeight, int maxHeight)
        : noise_(noise), scale_(scale), octaves_(octaves), threshold_(threshold), fillBlock_(fillBlock), startHeight_(startHeight), maxHeight_(maxHeight) {
        if (!noise_) throw std::invalid_argument("PerlinNoiseChunkTransform requires a valid PerlinNoise instance");
        if (scale_ <= 0.0) throw std::invalid_argument("PerlinNoiseChunkTransform requires scale > 0.0");
        if (octaves_ <= 0) throw std::invalid_argument("PerlinNoiseChunkTransform requires octaves > 0");
        if (threshold_ < 0.0 || threshold_ > 1.0) throw std::invalid_argument("PerlinNoiseChunkTransform requires threshold in [0.0, 1.0]");
        if (startHeight_ >= maxHeight_) throw std::invalid_argument("PerlinNoiseChunkTransform requires startHeight < maxHeight");
    }
    void apply(ChunkSpan& chunk) const override {
        auto absPos = chunkOrigin(chunk.position);
        for (int z = 0; z < CHUNK_DEPTH; ++z) {
            for (int y = 0; y < CHUNK_HEIGHT; ++y) {
                for (int x = 0; x < CHUNK_WIDTH; ++x) {
                    int index = x + y * chunk.strideY + z * chunk.strideZ;
                    // Only modify empty blocks
                    if (chunk.storage[index] != Block::Empty) continue;

                    int worldX = absPos.x + x;
                    int worldY = absPos.y + y;
                    int worldZ = absPos.z + z;

                    // Calculate noise value at this position
                    double noiseValue = noise_->normalizedOctave2D_01(worldX / scale_, worldZ / scale_, octaves_);
                    int height = static_cast<int>(noiseValue * (maxHeight_ - startHeight_)) + startHeight_;
                    if (worldY <= height && worldY <= maxHeight_) {
                        chunk.storage[index] = fillBlock_;
                    }
                }
            }
        }
    }
private:
    const std::shared_ptr<siv::PerlinNoise> noise_;
    const double scale_;
    const int octaves_;
    const double threshold_;
    const Block fillBlock_;
    const int startHeight_;
    const int maxHeight_;
};

class HeightmapChunkTransform : public ChunkTransform {
public:
    HeightmapChunkTransform(int height, Block fillBlock) : height_(height), fillBlock_(fillBlock) {}
    void apply(ChunkSpan& chunk) const override {
        auto absPos = chunkOrigin(chunk.position);
        int globalYStart = static_cast<int>(absPos.y);
        for (int y = 0; y < CHUNK_HEIGHT; ++y) {
            int globalY = globalYStart + y;
            if (globalY < height_) {
                for (int z = 0; z < CHUNK_DEPTH; ++z) {
                    for (int x = 0; x < CHUNK_WIDTH; ++x) {
                        chunk.storage[y * chunk.strideY + z * chunk.strideZ + x] = fillBlock_;
                    }
                }
            }
        }
    }
private:
    const int height_;
    const Block fillBlock_;
};