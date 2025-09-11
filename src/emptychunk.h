#pragma once
#include "chunkoverlay.h"

class EmptyChunk : public ChunkOverlay {
public:
    EmptyChunk() = default;
    Block getBlock(const ChunkLocalPosition pos) const override;
    ~EmptyChunk() override = default;
};
