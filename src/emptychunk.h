#pragma once
#include "chunkoverlay.h"

class EmptyChunk : public ChunkOverlay {
public:
    EmptyChunk() = default;
    Block getBlock(const ChunkLocalPosition pos, [[maybe_unused]] const Block parentLayerBlock = Block::Empty) const override;
    ~EmptyChunk() override = default;
};
