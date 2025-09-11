#pragma once
#include <memory>
#include <cstdint>
#include <optional>
#include "block.h"
#include "position.h"
#include "chunkdims.h"

class ChunkOverlay;
template<typename A, typename B>
class CombinedChunkOverlay;

class ChunkOverlay : public std::enable_shared_from_this<ChunkOverlay> {
public:
    virtual Block getBlock(const ChunkLocalPosition pos, const Block parentLayerBlock = Block::Empty) const = 0;
    virtual ~ChunkOverlay() = default;
    std::shared_ptr<ChunkOverlay> shared_from_this() {
        return std::enable_shared_from_this<ChunkOverlay>::shared_from_this();
    }
    std::shared_ptr<const ChunkOverlay> shared_from_this() const {
        return std::enable_shared_from_this<ChunkOverlay>::shared_from_this();
    }
    template<typename A, typename B>
    friend class CombinedChunkOverlay;
protected:
};

template<typename A, typename B>
class CombinedChunkOverlay : public ChunkOverlay {
public:
    CombinedChunkOverlay(std::shared_ptr<A> a, std::shared_ptr<B> b)
        : overlayA(a), overlayB(b) {}
    Block getBlock(const ChunkLocalPosition pos, const Block parentLayerBlock = Block::Empty) const override {
        Block pb = overlayB->getBlock(pos, parentLayerBlock);
        return overlayA->getBlock(pos, pb);
    }
    ~CombinedChunkOverlay() override = default;
protected:
    
    std::shared_ptr<A> overlayA;
    std::shared_ptr<B> overlayB;
};
template<typename A, typename B>
std::shared_ptr<CombinedChunkOverlay<A, B>> operator&(
    const std::shared_ptr<A>& a,
    const std::shared_ptr<B>& b
)
{
    return std::make_shared<CombinedChunkOverlay<A, B>>(a, b);
}