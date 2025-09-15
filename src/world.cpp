#include "world.h"
#include "chunkspan.h"
#include "chunktransform.h"
#include "name_component.h"
#include "player_session.h"
#include <cmath>
#include <algorithm>
#include <set>
#include <iostream>

World::World(
    std::shared_ptr<IWorldgenStrategy> chunkGenerator,
    const std::function<std::vector<AbsoluteBlockPosition>()>& loadAnchors,
    size_t loadAnchorRadiusInChunks,
    size_t seed,
    std::shared_ptr<IChunkPersistence> persistence)
    : chunkGenerator_(chunkGenerator), 
      loadAnchors_(loadAnchors), 
      loadAnchorRadiusInChunks_(loadAnchorRadiusInChunks), 
      seed_(seed), 
      persistence_(persistence) {
}

std::optional<std::shared_ptr<ChunkSpan>> World::chunkAt(const AbsoluteChunkPosition pos) const {
    auto it = chunks_.find(pos);
    if (it != chunks_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void World::ensureChunksLoaded() {
    if (!loadAnchors_) {
        return;
    }
    
    // Get load anchors
    std::vector<AbsoluteBlockPosition> anchors = loadAnchors_();

    // query for name components to load player entities in those chunks
    auto view = entityRegistry_.view<NameComponent, AbsolutePrecisePosition>();
    for (auto entity : view) {
        const auto& pos = view.get<AbsolutePrecisePosition>(entity);
        anchors.push_back(toAbsoluteBlock(pos));
    }

    
    // Convert anchors to chunk positions and collect all chunks to load
    std::set<AbsoluteChunkPosition, std::function<bool(const AbsoluteChunkPosition&, const AbsoluteChunkPosition&)>> 
        chunksToLoad([](const AbsoluteChunkPosition& a, const AbsoluteChunkPosition& b) {
            if (a.x != b.x) return a.x < b.x;
            if (a.y != b.y) return a.y < b.y;
            return a.z < b.z;
        });
    
    const int32_t radius = static_cast<int32_t>(loadAnchorRadiusInChunks_);
    
    for (const auto& anchor : anchors) {
        AbsoluteChunkPosition anchorChunk = toAbsoluteChunk(anchor);
        
        // Add all chunks within the radius
        for (int32_t dx = -radius; dx <= radius; dx++) {
            for (int32_t dy = -radius; dy <= radius; dy++) {
                for (int32_t dz = -radius; dz <= radius; dz++) {
                    // Check if within spherical radius
                    double distance = std::sqrt(dx*dx + dy*dy + dz*dz);
                    if (distance <= radius) {
                        AbsoluteChunkPosition chunkPos = {
                            anchorChunk.x + dx,
                            anchorChunk.y + dy,
                            anchorChunk.z + dz
                        };
                        chunksToLoad.insert(chunkPos);
                    }
                }
            }
        }
    }
    
    // Load chunks that aren't already loaded
    for (const auto& chunkPos : chunksToLoad) {
        if (chunks_.find(chunkPos) == chunks_.end()) {
            std::shared_ptr<ChunkSpan> chunk = nullptr;
            
            // Try to load from persistence first
            if (persistence_) {
                auto persistedChunk = persistence_->loadChunk(chunkPos);
                if (persistedChunk.has_value()) {
                    chunk = persistedChunk.value();
                }
            }
            
            // If not in persistence, generate the chunk
            if (!chunk && chunkGenerator_) {
                auto transform = chunkGenerator_->generateChunk(chunkPos, seed_);
                if (transform) {
                    // Create an empty chunk and apply the transform
                    chunk = std::make_shared<ChunkSpan>(chunkPos);
                    transform->apply(*chunk);
                }
            }
            
            // If we still don't have a chunk, create an empty one
            if (!chunk) {
                chunk = std::make_shared<ChunkSpan>(chunkPos);
            }
            
            chunks_[chunkPos] = chunk;
        }
    }
}

void World::garbageCollectChunks() {
    if (!loadAnchors_) {
        return;
    }
    
    // Get load anchors
    std::vector<AbsoluteBlockPosition> anchors = loadAnchors_();
    
    // Convert anchors to chunk positions
    std::vector<AbsoluteChunkPosition> anchorChunks;
    for (const auto& anchor : anchors) {
        anchorChunks.push_back(toAbsoluteChunk(anchor));
    }
    
    const double radius = static_cast<double>(loadAnchorRadiusInChunks_);
    
    // Find chunks to unload
    std::vector<AbsoluteChunkPosition> chunksToUnload;
    
    for (const auto& [chunkPos, chunk] : chunks_) {
        bool shouldKeep = false;
        
        // Check if chunk is within radius of any anchor
        for (const auto& anchorChunk : anchorChunks) {
            double dx = chunkPos.x - anchorChunk.x;
            double dy = chunkPos.y - anchorChunk.y;
            double dz = chunkPos.z - anchorChunk.z;
            double distance = std::sqrt(dx*dx + dy*dy + dz*dz);
            
            if (distance <= radius) {
                shouldKeep = true;
                break;
            }
        }
        
        if (!shouldKeep) {
            chunksToUnload.push_back(chunkPos);
        }
    }
    
    // Save chunks to persistence and unload them
    for (const auto& chunkPos : chunksToUnload) {
        auto it = chunks_.find(chunkPos);
        if (it != chunks_.end()) {
            // Save to persistence if available
            if (persistence_) {
                persistence_->saveChunk(*it->second);
            }
            
            // Remove from memory
            chunks_.erase(it);
        }
    }
}

const std::optional<std::shared_ptr<const ChunkSpan>> World::getChunkIfLoaded(const AbsoluteChunkPosition& pos) const {
    auto it = chunks_.find(pos);
    if (it != chunks_.end()) {
        return std::const_pointer_cast<const ChunkSpan>(it->second);
    }
    return std::nullopt;
}

const std::optional<Block> World::getBlockIfLoaded(const AbsoluteBlockPosition& pos) const {
    // Convert to chunk position
    AbsoluteChunkPosition chunkPos = toAbsoluteChunk(pos);
    
    // Check if chunk is loaded
    auto chunkOpt = getChunkIfLoaded(chunkPos);
    if (!chunkOpt.has_value()) {
        return std::nullopt;
    }
    
    const auto& chunk = chunkOpt.value();
    
    // Convert to local position within the chunk
    ChunkLocalPosition localPos = toChunkLocal(pos, chunkPos);
    
    // Get the block from the chunk
    return chunk->getBlock(localPos);
}

World::~World() {
    // Save all loaded chunks to persistence before destroying
    if (persistence_) {
        persistence_->saveAllLoadedChunks(chunks_);
    }
}

bool World::setBlockIfLoaded(const AbsoluteBlockPosition& pos, Block block) {
    // Convert to chunk position
    AbsoluteChunkPosition chunkPos = toAbsoluteChunk(pos);
    
    // Check if chunk is loaded (get mutable reference)
    auto it = chunks_.find(chunkPos);
    if (it == chunks_.end()) {
        return false;
    }
    
    auto& chunk = it->second;
    
    // Convert to local position within the chunk
    ChunkLocalPosition localPos = toChunkLocal(pos, chunkPos);
    
    // Set the block in the chunk
    chunk->setBlock(localPos, block);
    
    return true;
}

entt::entity World::spawnPlayer(const std::string& playerName, const AbsolutePrecisePosition& position) {
    // Create a new entity in the registry
    entt::entity playerEntity = entityRegistry_.create();
    
    // Add components to the player entity
    entityRegistry_.emplace<NameComponent>(playerEntity, playerName);
    entityRegistry_.emplace<AbsolutePrecisePosition>(playerEntity, position);
    if (entityUpdatedCallback_) {
        entityUpdatedCallback_(playerEntity, entityRegistry_);
    }
    
    return playerEntity;
}

void World::despawnPlayer(entt::entity playerEntity) {
    // Check if entity exists before destroying
    if (entityRegistry_.valid(playerEntity)) {
        entityRegistry_.destroy(playerEntity);
    }
}

entt::entity World::connectPlayer(const std::string& playerName, const AbsolutePrecisePosition& spawnPosition) {
    // For now, connecting a player is the same as spawning them
    // In the future, this could include additional logic like:
    // - Loading player data from persistence
    // - Sending welcome messages
    // - Notifying other players
    return spawnPlayer(playerName, spawnPosition);
}

void World::disconnectPlayer(entt::entity playerEntity) {
    // For now, disconnecting a player is the same as despawning them
    // In the future, this could include additional logic like:
    // - Saving player data to persistence
    // - Notifying other players
    // - Cleanup of player-specific resources
    despawnPlayer(playerEntity);
}

std::string World::createPlayerSession(const std::string& playerName, const AbsolutePrecisePosition& spawnPosition) {
    // Spawn the player entity
    entt::entity playerEntity = spawnPlayer(playerName, spawnPosition);
    
    // Create session
    std::string sessionToken = sessionManager_.createSession(playerName, playerEntity, spawnPosition);
    
    return sessionToken;
}

bool World::refreshPlayerSession(const std::string& sessionToken) {
    return sessionManager_.refreshSession(sessionToken);
}

bool World::updatePlayerPosition(const std::string& sessionToken, const AbsolutePrecisePosition& position) {
    // Update position in session manager
    if (!sessionManager_.updatePlayerPosition(sessionToken, position)) {
        return false;
    }
    
    // Update position in entity registry
    auto sessionOpt = sessionManager_.getSession(sessionToken);
    std::clog << "Updating position for session: " << sessionToken << std::endl;
    if (sessionOpt.has_value()) {
        const auto& session = sessionOpt.value();
        if (entityRegistry_.valid(session.playerEntity)) {
            auto* posComponent = entityRegistry_.try_get<AbsolutePrecisePosition>(session.playerEntity);
            if (posComponent) {
                std::clog << " - Old Position: (" << posComponent->x << ", " << posComponent->y << ", " << posComponent->z << ")" << std::endl;
                std::clog << " - New Position: (" << position.x << ", " << position.y << ", " << position.z << ")" << std::endl;
                *posComponent = position;
                if (entityUpdatedCallback_) {
                    entityUpdatedCallback_(session.playerEntity, entityRegistry_);
                }
                return true;
            }
        }
    }
    
    return false;
}

bool World::isValidSession(const std::string& sessionToken) const {
    return sessionManager_.isValidSession(sessionToken);
}

std::optional<PlayerSession> World::getPlayerSession(const std::string& sessionToken) const {
    return sessionManager_.getSession(sessionToken);
}

std::vector<PlayerSession> World::getAllActiveSessions() const {
    return sessionManager_.getAllActiveSessions();
}

void World::disconnectPlayerBySession(const std::string& sessionToken) {
    auto sessionOpt = sessionManager_.getSession(sessionToken);
    if (sessionOpt.has_value()) {
        const auto& session = sessionOpt.value();
        disconnectPlayer(session.playerEntity);
        sessionManager_.removeSession(sessionToken);
    }
}

void World::cleanupExpiredSessions() {
    auto expiredTokens = sessionManager_.removeExpiredSessions();
    
    // Despawn entities for expired sessions
    for (const auto& token : expiredTokens) {
        // Session is already removed, but we need to clean up the entity
        // This is a bit tricky since we don't have the entity ID anymore
        // In a real implementation, you might want to store entity mappings separately
        // For now, we'll rely on the session cleanup
    }
}