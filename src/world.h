#pragma once

#include <memory>
#include <optional>
#include <unordered_map>
#include <functional>
#include <vector>

#include <entt/entt.hpp>

#include "chunktransform.h"
#include "position.h"
#include "name_component.h"
#include "player_session.h"
#include <functional>


struct ChunkPosHash {
    std::size_t operator()(const AbsoluteChunkPosition& pos) const {
        // Simple hash combining x, y, z
        return std::hash<int32_t>()(pos.x) ^ (std::hash<int32_t>()(pos.y) << 1) ^ (std::hash<int32_t>()(pos.z) << 2);
    }
};

struct ChunkPosEq {
    bool operator()(const AbsoluteChunkPosition& a, const AbsoluteChunkPosition& b) const {
        return a.x == b.x && a.y == b.y && a.z == b.z;
    }
};


// Type alias for chunk map
using ChunkMap = std::unordered_map<AbsoluteChunkPosition, std::shared_ptr<ChunkSpan>, ChunkPosHash, ChunkPosEq>;

// Interface for chunk generation
class IWorldgenStrategy {
public:
    virtual ~IWorldgenStrategy() = default;
    /**
     * @brief Returns a single ChunkTransform pointer for a given position. Must not mutate internal state, to prevent the same seed producing different results.
     * @param pos The absolute chunk position to generate.
     * @param seed A simple seed value for procedural generation.
     * @return A unique pointer to a ChunkTransform that can be applied to a ChunkSpan.
     */
    virtual std::shared_ptr<ChunkTransform> generateChunk(const AbsoluteChunkPosition& pos, size_t seed) const = 0;
};

// Interface for chunk persistence
class IChunkPersistence {
public:
    virtual ~IChunkPersistence() = default;
    virtual bool saveChunk(const ChunkSpan& chunk) = 0;
    virtual std::optional<std::shared_ptr<ChunkSpan>> loadChunk(const AbsoluteChunkPosition& pos) = 0;
    virtual void saveAllLoadedChunks(const ChunkMap& chunks) = 0;
};

class World {
public:
    World(
        std::shared_ptr<IWorldgenStrategy> chunkGenerator = nullptr,
        const std::function<std::vector<AbsoluteBlockPosition>()>& loadAnchors = []() { return std::vector<AbsoluteBlockPosition>{ {0,0,0} }; },
        size_t loadAnchorRadiusInChunks = 10,
        size_t seed = 0,
        std::shared_ptr<IChunkPersistence> persistence = nullptr);
    std::optional<std::shared_ptr<ChunkSpan>> chunkAt(const AbsoluteChunkPosition pos) const;
    void ensureChunksLoaded();
    void garbageCollectChunks();
    // note users can NEVER force-load a chunk, they can only set anchors and call ensureChunksLoaded()
    const std::optional<std::shared_ptr<const ChunkSpan>> getChunkIfLoaded(const AbsoluteChunkPosition& pos) const;
    const std::optional<Block> getBlockIfLoaded(const AbsoluteBlockPosition& pos) const;
    bool setBlockIfLoaded(const AbsoluteBlockPosition& pos, Block block);
    
    // Player management methods
    entt::entity spawnPlayer(const std::string& playerName, const AbsolutePrecisePosition& position);
    void despawnPlayer(entt::entity playerEntity);
    entt::entity connectPlayer(const std::string& playerName, const AbsolutePrecisePosition& spawnPosition);
    void disconnectPlayer(entt::entity playerEntity);
    
    // Session management methods
    std::string createPlayerSession(const std::string& playerName, const AbsolutePrecisePosition& spawnPosition);
    bool refreshPlayerSession(const std::string& sessionToken);
    bool updatePlayerPosition(const std::string& sessionToken, const AbsolutePrecisePosition& position);
    bool isValidSession(const std::string& sessionToken) const;
    std::optional<PlayerSession> getPlayerSession(const std::string& sessionToken) const;
    std::vector<PlayerSession> getAllActiveSessions() const;
    void disconnectPlayerBySession(const std::string& sessionToken);
    void cleanupExpiredSessions();

    // Entity update callback (called when an entity with a position is updated/spawned)
    void setEntityUpdatedCallback(const std::function<void(entt::entity, const entt::registry&)>& cb) { entityUpdatedCallback_ = cb; }
    size_t getLoadAnchorRadiusInChunks() const { return loadAnchorRadiusInChunks_; }
    
    // Registry access
    entt::registry& getRegistry() { return entityRegistry_; }
    const entt::registry& getRegistry() const { return entityRegistry_; }
    
    ~World();
private:
    // Map of loaded chunks
    ChunkMap chunks_;
    // Chunk generator for creating chunks on demand
    std::shared_ptr<IWorldgenStrategy> chunkGenerator_;
    // Load anchors and radius
    std::function<std::vector<AbsoluteBlockPosition>()> loadAnchors_;
    size_t loadAnchorRadiusInChunks_ = 10;
    // Simple seed for procedural generation
    size_t seed_ = 0;
    // Persistence provider (can be null)
    std::shared_ptr<IChunkPersistence> persistence_;
    //entt registry for entities
    entt::registry entityRegistry_;
    // Player session manager
    PlayerSessionManager sessionManager_;
    // Callback for notifying server of entity updates
    std::function<void(entt::entity, const entt::registry&)> entityUpdatedCallback_;
};