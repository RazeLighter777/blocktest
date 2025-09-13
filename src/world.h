#pragma once

#include <memory>
#include <optional>
#include <unordered_map>
#include <functional>
#include <vector>
#include "chunkoverlay.h"
#include "position.h"

// Hash and equality functors for AbsoluteChunkPosition
struct ChunkPosHash {
    std::size_t operator()(const AbsoluteChunkPosition& p) const noexcept {
        uint64_t x = static_cast<uint32_t>(p.x);
        uint64_t y = static_cast<uint32_t>(p.y);
        uint64_t z = static_cast<uint32_t>(p.z);
        uint64_t h = x;
        h = (h * 0x9E3779B97F4A7C15ull) ^ (y + 0x85EBCA77C2B2AE63ull + (h << 6) + (h >> 2));
        h = (h * 0xC2B2AE3D27D4EB4Full) ^ (z + 0x165667B19E3779F9ull + (h << 6) + (h >> 2));
        return static_cast<std::size_t>(h ^ (h >> 32));
    }
};

struct ChunkPosEq {
    bool operator()(const AbsoluteChunkPosition& a, const AbsoluteChunkPosition& b) const noexcept {
        return a.x == b.x && a.y == b.y && a.z == b.z;
    }
};

// Type alias for chunk map
using ChunkMap = std::unordered_map<AbsoluteChunkPosition, std::shared_ptr<ChunkSpan>, ChunkPosHash, ChunkPosEq>;

// Interface for chunk generation
class IChunkGenerator {
public:
    virtual ~IChunkGenerator() = default;
    virtual std::unique_ptr<ChunkSpan> generateChunk(const AbsoluteChunkPosition& pos, size_t seed) = 0;
};

// Interface for chunk persistence
class IChunkPersistence {
public:
    virtual ~IChunkPersistence() = default;
    virtual bool saveChunk(const AbsoluteChunkPosition& pos, const ChunkSpan& chunk) = 0;
    virtual std::optional<std::shared_ptr<ChunkSpan>> loadChunk(const AbsoluteChunkPosition& pos) = 0;
    virtual void saveAllLoadedChunks(const ChunkMap& chunks) = 0;
};

class World {
public:
    World(
        std::shared_ptr<IChunkGenerator> chunkGenerator = nullptr,
        const std::vector<AbsoluteBlockPosition>& loadAnchors = {},
        size_t loadAnchorRadiusInChunks = 10,
        size_t seed = 0,
        std::shared_ptr<IChunkPersistence> persistence = nullptr);
    std::optional<std::shared_ptr<ChunkSpan>> chunkAt(const AbsoluteChunkPosition pos) const;
    void ensureChunksLoaded();
    ~World();
    // Helper methods for chunk persistence (delegated)
    bool saveChunk(const AbsoluteChunkPosition& pos, const ChunkSpan& chunk);
    std::optional<std::shared_ptr<ChunkSpan>> loadChunk(const AbsoluteChunkPosition& pos);
    void saveAllLoadedChunks();
private:
    // Map of loaded chunks
    ChunkMap chunks_;
    // Chunk generator for creating chunks on demand
    std::shared_ptr<IChunkGenerator> chunkGenerator_;
    // Load anchors and radius
    std::vector<AbsoluteBlockPosition> loadAnchors_;
    size_t loadAnchorRadiusInChunks_ = 10;
    // Simple seed for procedural generation
    size_t seed_ = 0;
    // Persistence provider (can be null)
    std::shared_ptr<IChunkPersistence> persistence_;
};