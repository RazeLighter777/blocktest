#include "chunk_generators.h"
#include "chunkdims.h"
#include <algorithm>


std::shared_ptr<ChunkTransform> FlatworldChunkGenerator::generateChunk(const AbsoluteChunkPosition& pos, [[maybe_unused]] size_t seed) const {
    // Create a transform that fills up to the specified height with the fill block
    return std::make_shared<HeightmapChunkTransform>(static_cast<int>(height_), fillBlock_);
};