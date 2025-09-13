// World implementation
#include "world.h"
#include "emptychunk.h"
#include "statefulchunkoverlay.h"
#include <vector>
#include <utility>
#include <iostream>

// We use custom hash/eq functors in world.h for AbsoluteChunkPosition keys.

namespace {
// A ChunkSpan that owns its storage so data remains valid while referenced.
struct OwningChunkSpan : public ChunkSpan {
    std::vector<Block> storage;
    explicit OwningChunkSpan(const AbsoluteChunkPosition pos)
        : ChunkSpan(pos), storage(kChunkElemCount, Block::Empty) {
        data = storage.data();
        // Keep default strides from base: strideY=CHUNK_WIDTH, strideZ=CHUNK_WIDTH*CHUNK_HEIGHT
    }
};

// A ChunkSpan that can be persisted using StatefulChunkOverlay
struct PersistentChunkSpan : public OwningChunkSpan {
    StatefulChunkOverlay overlay;
    
    explicit PersistentChunkSpan(const AbsoluteChunkPosition pos) 
        : OwningChunkSpan(pos) {}
        
    explicit PersistentChunkSpan(const AbsoluteChunkPosition pos, const StatefulChunkOverlay& overlay)
        : OwningChunkSpan(pos), overlay(overlay) {
        // Generate the chunk data from the overlay
        overlay.generate(*this);
    }
    
    // Update the overlay when chunk data changes
    void updateOverlay() {
        overlay = StatefulChunkOverlay();
        for (uint32_t z = 0; z < CHUNK_DEPTH; ++z) {
            for (uint32_t y = 0; y < CHUNK_HEIGHT; ++y) {
                for (uint32_t x = 0; x < CHUNK_WIDTH; ++x) {
                    const std::size_t idx = static_cast<std::size_t>(z) * strideZ + 
                                          static_cast<std::size_t>(y) * strideY + x;
                    const Block block = data[idx];
                    if (block != Block::Empty) {
                        overlay.setBlock(ChunkLocalPosition(x, y, z), block);
                    }
                }
            }
        }
    }
};
}

World::World(
    std::shared_ptr<IChunkGenerator> chunkGenerator,
    const std::vector<AbsoluteBlockPosition>& loadAnchors,
    size_t loadAnchorRadiusInChunks,
    size_t seed,
    std::shared_ptr<IChunkPersistence> persistence)
    : chunkGenerator_(std::move(chunkGenerator)),
      loadAnchors_(loadAnchors),
      loadAnchorRadiusInChunks_(loadAnchorRadiusInChunks),
      seed_(seed),
      persistence_(std::move(persistence)) {
}World::~World() {
    saveAllLoadedChunks();
}

void World::ensureChunksLoaded() {
    if (loadAnchors_.empty()) return;

    const int32_t r = static_cast<int32_t>(loadAnchorRadiusInChunks_);

    for (const auto& anchorBlockPos : loadAnchors_) {
        const AbsoluteChunkPosition anchorChunk = toAbsoluteChunk(anchorBlockPos);
        for (int32_t dz = -r; dz <= r; ++dz) {
            for (int32_t dy = -r; dy <= r; ++dy) {
                for (int32_t dx = -r; dx <= r; ++dx) {
                    AbsoluteChunkPosition cp(
                        anchorChunk.x + dx,
                        anchorChunk.y + dy,
                        anchorChunk.z + dz
                    );
                    if (chunks_.find(cp) != chunks_.end()) continue;

                    std::shared_ptr<ChunkSpan> spanPtr;

                    // First try to load from persistence
                    if (persistence_) {
                        spanPtr = loadChunk(cp).value_or(nullptr);
                    }

                    if (!spanPtr && chunkGenerator_) {
                        // Allow custom generator; wrap unique_ptr into shared_ptr
                        std::unique_ptr<ChunkSpan> up = chunkGenerator_->generateChunk(cp, seed_);
                        if (up) {
                            spanPtr = std::shared_ptr<ChunkSpan>(up.release());
                        }
                    }

                    if (!spanPtr) {
                        // Fallback: allocate an empty, owning chunk
                        spanPtr = std::make_shared<PersistentChunkSpan>(cp);
                    }

                    chunks_.emplace(cp, std::move(spanPtr));
                }
            }
        }
    }
    // Save and unload chunks outside the radius
    std::vector<AbsoluteChunkPosition> toUnload;
    for (const auto& kv : chunks_) {
        const AbsoluteChunkPosition& cp = kv.first;
        bool withinAnyAnchor = false;
        for (const auto& anchorBlockPos : loadAnchors_) {
            const AbsoluteChunkPosition anchorChunk = toAbsoluteChunk(anchorBlockPos);
            if (std::abs(cp.x - anchorChunk.x) <= r &&
                std::abs(cp.y - anchorChunk.y) <= r &&
                std::abs(cp.z - anchorChunk.z) <= r) {
                withinAnyAnchor = true;
                break;
            }
        }
        if (!withinAnyAnchor) {
            toUnload.push_back(cp);
        }
    }
    for (const auto& cp : toUnload) {
        auto it = chunks_.find(cp);
        if (it != chunks_.end()) {
            saveChunk(cp, *it->second);
            chunks_.erase(it);
        }
    }
}

std::optional<std::shared_ptr<ChunkSpan>> World::chunkAt(const AbsoluteChunkPosition pos) const {
    auto it = chunks_.find(pos);
    if (it == chunks_.end()) return std::nullopt;
    return it->second;
}

// No longer needed: initializeDatabase

bool World::saveChunk(const AbsoluteChunkPosition& pos, const ChunkSpan& chunk) {
    if (persistence_) {
        return persistence_->saveChunk(pos, chunk);
    }
    return false;
}

std::optional<std::shared_ptr<ChunkSpan>> World::loadChunk(const AbsoluteChunkPosition& pos) {
    if (persistence_) {
        return persistence_->loadChunk(pos);
    }
    return std::nullopt;
}

void World::saveAllLoadedChunks() {
    if (persistence_) {
        persistence_->saveAllLoadedChunks(chunks_);
    }
}