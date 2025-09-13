#pragma once
#include "chunkoverlay.h"

class EmptyChunk : public ChunkOverlay {
public:
    EmptyChunk() = default;
    ~EmptyChunk() override = default;

    // Always produce Empty, ignoring any parent input.
    void generateInto(const ChunkSpan& out, const Block* /*parent*/) const override {
        std::fill(out.data, out.data + kChunkElemCount, Block::Empty);
    }
};
