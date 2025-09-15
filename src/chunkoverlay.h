#pragma once
#include <memory>
#include <cstdint>
#include <vector>
#include <algorithm>
#include "block.h"
#include "position.h"
#include "chunkdims.h"
#include "position.h"
// A span describing a chunk-shaped writable view. Strides are in elements.
struct ChunkSpan {
    Block* data = nullptr;
    const std::uint32_t strideY = CHUNK_WIDTH;                   // distance between consecutive y elements
    const std::uint32_t strideZ = CHUNK_WIDTH * CHUNK_HEIGHT;    // distance between consecutive z slices
    AbsoluteChunkPosition position{0,0,0}; // Optional: position of this chunk in world space
    ChunkSpan() = default;
    ChunkSpan(AbsoluteChunkPosition pos) : position(pos) {}
    ChunkSpan(Block* d, std::uint32_t sy, std::uint32_t sz, AbsoluteChunkPosition pos = {0,0,0})
        : data(d), strideY(sy), strideZ(sz), position(pos) {}
    virtual ~ChunkSpan() = default; // Make it polymorphic for dynamic_cast
};

static constexpr std::size_t kChunkElemCount =
    static_cast<std::size_t>(CHUNK_WIDTH) * CHUNK_HEIGHT * CHUNK_DEPTH;

class ChunkOverlay : public std::enable_shared_from_this<ChunkOverlay> {
public:
    virtual ~ChunkOverlay() = default;

    // Core operation: generate this overlay into 'out' using 'parent' as the input (may be null for Empty base).
    // Default behavior is passthrough/copy of parent when provided, or fill with Empty when parent is null.
    virtual void generateInto(const ChunkSpan& out, const Block* parent) const {
        if (parent) {
            // Copy parent into out (passthrough)
            std::copy(parent, parent + kChunkElemCount, out.data);
        } else {
            // Fill with Empty
            std::fill(out.data, out.data + kChunkElemCount, Block::Empty);
        }
    }

    // Convenience: generate this overlay on top of an Empty base.
    virtual void generate(const ChunkSpan& out) const {
        // Base is Empty: pass nullptr to indicate Empty base to generateInto
        generateInto(out, nullptr);
    }
};

// A flattened chain of overlays (top-most first) computed in chunk-wide passes.
class ChainOverlay : public ChunkOverlay {
public:
    explicit ChainOverlay(std::vector<std::shared_ptr<const ChunkOverlay>> layers)
        : layers_(std::move(layers)) {}

    // Generate the composed result into 'out'.
    void generate(const ChunkSpan& out) const override {
        if (layers_.empty()) {
            // Empty composition => fill with Empty.
            std::fill(out.data, out.data + kChunkElemCount, Block::Empty);
            return;
        }

    // We'll keep a rolling parent buffer. Start with Empty base.
    std::vector<Block> parentBuf(kChunkElemCount, Block::Empty);

        // Apply layers from bottom-most to top-most. Our 'layers_' are top-most first, so iterate in reverse.
        // For all but the final (top-most), write into a temporary buffer and swap.
        std::vector<Block> workBuf(kChunkElemCount);
        for (std::size_t i = layers_.size(); i-- > 0;) {
            const bool isTopMost = (i == 0);
            if (isTopMost) {
                // Write final result directly into out using current parentBuf as parent
                layers_[i]->generateInto(out, parentBuf.data());
            } else {
                ChunkSpan workSpan{ workBuf.data(), CHUNK_WIDTH, static_cast<std::uint32_t>(CHUNK_WIDTH) * CHUNK_HEIGHT };
                layers_[i]->generateInto(workSpan, parentBuf.data());
                parentBuf.swap(workBuf);
            }
        }
    }

private:
    std::vector<std::shared_ptr<const ChunkOverlay>> layers_;
};

// Helpers to compose overlays into a flattened chain (top-most first).
inline std::shared_ptr<ChainOverlay> compose(std::vector<std::shared_ptr<const ChunkOverlay>> layers) {
    return std::make_shared<ChainOverlay>(std::move(layers));
}

template <typename... T>
inline std::shared_ptr<ChainOverlay> compose(const std::shared_ptr<T>&... layers) {
    std::vector<std::shared_ptr<const ChunkOverlay>> v{ layers... };
    return std::make_shared<ChainOverlay>(std::move(v));
}
