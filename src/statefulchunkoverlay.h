#pragma once
#include <unordered_map>
#include <cstdint>
#include <memory>
#include "chunkoverlay.h"
#include <vector>
#include <optional>

// A sparse, mutable chunk overlay that stores only non-Empty blocks.
class StatefulChunkOverlay : public ChunkOverlay {
public:
    StatefulChunkOverlay() = default;
    // Encodes an existing overlay into a stateful one (captures its generated result).
    StatefulChunkOverlay(const ChunkOverlay& other);
    ~StatefulChunkOverlay() override = default;

    // Chunk-wide overlay: start from parent (or Empty) and apply our sparse edits.
    void generateInto(const ChunkSpan& out, const Block* parent) const override;

    // Set a block at the given local position. Setting Empty removes the entry.
    void setBlock(const ChunkLocalPosition pos, Block block);

    // Serialize to a compact binary representation.
    std::vector<uint8_t> serialize() const;

    // Deserialize from a compact binary representation.
    // Returns std::nullopt if the data is invalid or incompatible.
    static std::optional<StatefulChunkOverlay> deserialize(const std::vector<uint8_t>& data);

private:
    static constexpr uint32_t packKey(const ChunkLocalPosition pos) {
        return (static_cast<uint32_t>(pos.x) << 16) |
               (static_cast<uint32_t>(pos.y) << 8)  |
               static_cast<uint32_t>(pos.z);
    }

    std::unordered_map<uint32_t, Block> blocks_; // key: packed (x,y,z)
};
