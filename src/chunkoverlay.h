#pragma once
#include <memory>
#include <cstdint>
#include <optional>
#include "block.h"
#include "position.h"
constexpr uint8_t CHUNK_WIDTH = 255;
constexpr uint8_t CHUNK_HEIGHT = 255;
constexpr uint8_t CHUNK_DEPTH = 255;
typedef Block ChunkBlocks[CHUNK_WIDTH][CHUNK_HEIGHT][CHUNK_DEPTH];

class ChunkOverlay;
template<typename A, typename B>
class CombinedChunkOverlay;

class ChunkOverlay : public std::enable_shared_from_this<ChunkOverlay> {
public:
    virtual Block getBlock(const ChunkLocalPosition pos) const = 0;
    virtual ~ChunkOverlay() = default;
    virtual std::shared_ptr<ChunkOverlay> shared_from_this() {
        return std::enable_shared_from_this<ChunkOverlay>::shared_from_this();
    }
    virtual std::shared_ptr<const ChunkOverlay> shared_from_this() const {
        return std::enable_shared_from_this<ChunkOverlay>::shared_from_this();
    }
    Block getPreviousBlock(const ChunkLocalPosition pos) const {
        if (previousOverlay_) {
            return previousOverlay_.value()->getBlock(pos);
        }
        return Block::Empty;
    }
    template<typename A, typename B>
    friend class CombinedChunkOverlay;
protected:
    std::optional<std::shared_ptr<ChunkOverlay>> previousOverlay_ = std::nullopt;
};

template<typename A, typename B>
class CombinedChunkOverlay : public ChunkOverlay {
public:
    CombinedChunkOverlay(std::shared_ptr<A> a, std::shared_ptr<B> b)
        : overlayA(a), overlayB(b) {
            overlayA->previousOverlay_ = overlayB;
    }
    std::shared_ptr<A> overlayA;
    std::shared_ptr<B> overlayB;
    Block getBlock(const ChunkLocalPosition pos) const override {
        auto block = overlayA->getBlock(pos);
        if (block != Block::Empty) {
            return block;
        }
        return overlayB->getBlock(pos);
    }
    ~CombinedChunkOverlay() override = default;
protected:

};
template<typename A, typename B>
std::shared_ptr<CombinedChunkOverlay<A, B>> operator&(
    const std::shared_ptr<A>& a,
    const std::shared_ptr<B>& b
)
{
    return std::make_shared<CombinedChunkOverlay<A, B>>(a, b);
}