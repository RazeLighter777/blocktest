#include "client.h"
#include "statefulchunkoverlay.h"
#include "chunkdims.h"
#include <iostream>
#include <algorithm>
#include <chrono>

// Helper class for chunk ownership that's compatible with existing serialization
class SerializableChunkSpan : public ChunkSpan {
public:
    std::vector<Block> storage;
    StatefulChunkOverlay overlay;
    
    explicit SerializableChunkSpan(const AbsoluteChunkPosition& pos)
        : ChunkSpan(pos), storage(kChunkElemCount, Block::Empty) {
        data = storage.data();
        // Default strides: strideY=CHUNK_WIDTH, strideZ=CHUNK_WIDTH*CHUNK_HEIGHT
    }
    
    // Create from deserialized overlay data
    SerializableChunkSpan(const AbsoluteChunkPosition& pos, const StatefulChunkOverlay& overlay)
        : ChunkSpan(pos), storage(kChunkElemCount, Block::Empty), overlay(overlay) {
        data = storage.data();
        // Generate chunk data from overlay
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
    
    std::vector<uint8_t> serialize() const {
        return overlay.serialize();
    }
};

Client::Client(const std::string& host, uint16_t port)
    : host_(host), port_(port), connected_(false) {
}

Client::~Client() {
    disconnect();
}

bool Client::connect() {
    try {
        rpcClient_ = std::make_unique<rpc::client>(host_, port_);
        
        // Test connection with a ping
        connected_ = ping();
        return connected_;
    } catch (const std::exception& e) {
        handleRpcError(e);
        connected_ = false;
        return false;
    }
}

void Client::disconnect() {
    if (rpcClient_) {
        rpcClient_.reset();
    }
    connected_ = false;
}

bool Client::isConnected() const {
    return connected_ && rpcClient_ != nullptr;
}

std::optional<std::shared_ptr<ChunkSpan>> Client::requestChunk(const AbsoluteChunkPosition& pos) {
    // Check cache first
    auto cached = getCachedChunk(pos);
    if (cached) {
        return cached;
    }
    
    if (!isConnected()) {
        std::cerr << "Client not connected" << std::endl;
        return std::nullopt;
    }
    
    try {
        // Request chunk from server - server returns serialized StatefulChunkOverlay
        auto result = rpcClient_->call("get_chunk", pos.x, pos.y, pos.z);
        auto serializedData = result.as<std::vector<uint8_t>>();
        
        // Deserialize the overlay
        auto overlayOpt = StatefulChunkOverlay::deserialize(serializedData);
        if (!overlayOpt) {
            std::cerr << "Failed to deserialize chunk data for position (" 
                      << pos.x << ", " << pos.y << ", " << pos.z << ")" << std::endl;
            return std::nullopt;
        }
        
        // Create chunk from overlay
        auto chunk = std::make_shared<SerializableChunkSpan>(pos, *overlayOpt);
        
        // Cache the chunk
        cacheChunk(pos, chunk);
        
        return chunk;
    } catch (const std::exception& e) {
        handleRpcError(e);
        return std::nullopt;
    }
}

void Client::preloadChunksAroundPosition(const AbsoluteBlockPosition& position, size_t radiusInChunks) {
    const auto centerChunk = toAbsoluteChunk(position);
    const int32_t radius = static_cast<int32_t>(radiusInChunks);
    
    for (int32_t dx = -radius; dx <= radius; ++dx) {
        for (int32_t dy = -radius; dy <= radius; ++dy) {
            for (int32_t dz = -radius; dz <= radius; ++dz) {
                AbsoluteChunkPosition chunkPos = {
                    centerChunk.x + dx,
                    centerChunk.y + dy,
                    centerChunk.z + dz
                };
                
                // Request chunk (this will cache it automatically)
                requestChunk(chunkPos);
            }
        }
    }
}

bool Client::placeBlock(const AbsoluteBlockPosition& pos, Block block) {
    if (!isConnected()) {
        std::cerr << "Client not connected" << std::endl;
        return false;
    }
    
    try {
        auto result = rpcClient_->call("place_block", pos.x, pos.y, pos.z, static_cast<uint8_t>(block));
        bool success = result.as<bool>();
        
        if (success) {
            // Update local cache if chunk is loaded
            auto chunkPos = toAbsoluteChunk(pos);
            auto cached = getCachedChunk(chunkPos);
            if (cached) {
                auto localPos = toChunkLocal(pos, chunkPos);
                const std::size_t idx = static_cast<std::size_t>(localPos.z) * cached->strideZ + 
                                      static_cast<std::size_t>(localPos.y) * cached->strideY + localPos.x;
                cached->data[idx] = block;
                
                // Update overlay if it's a SerializableChunkSpan
                auto serializableChunk = std::dynamic_pointer_cast<SerializableChunkSpan>(cached);
                if (serializableChunk) {
                    serializableChunk->updateOverlay();
                }
            }
        }
        
        return success;
    } catch (const std::exception& e) {
        handleRpcError(e);
        return false;
    }
}

bool Client::breakBlock(const AbsoluteBlockPosition& pos) {
    return placeBlock(pos, Block::Empty);
}

std::optional<Block> Client::getBlockAt(const AbsoluteBlockPosition& pos) {
    // First check local cache
    auto chunkPos = toAbsoluteChunk(pos);
    auto cached = getCachedChunk(chunkPos);
    if (cached) {
        auto localPos = toChunkLocal(pos, chunkPos);
        const std::size_t idx = static_cast<std::size_t>(localPos.z) * cached->strideZ + 
                              static_cast<std::size_t>(localPos.y) * cached->strideY + localPos.x;
        return cached->data[idx];
    }
    
    // If not in cache, request the chunk
    auto chunk = requestChunk(chunkPos);
    if (chunk) {
        auto localPos = toChunkLocal(pos, chunkPos);
        const std::size_t idx = static_cast<std::size_t>(localPos.z) * (*chunk)->strideZ + 
                              static_cast<std::size_t>(localPos.y) * (*chunk)->strideY + localPos.x;
        return (*chunk)->data[idx];
    }
    
    return std::nullopt;
}

void Client::clearCache() {
    cachedChunks_.clear();
}

std::optional<std::shared_ptr<ChunkSpan>> Client::getCachedChunk(const AbsoluteChunkPosition& pos) const {
    auto it = cachedChunks_.find(pos);
    if (it != cachedChunks_.end()) {
        return it->second;
    }
    return std::nullopt;
}

size_t Client::getCacheSize() const {
    return cachedChunks_.size();
}

void Client::evictOldChunks(size_t maxChunks) {
    if (cachedChunks_.size() <= maxChunks) {
        return;
    }
    
    // Simple eviction: remove excess chunks (in a real implementation, you might use LRU)
    size_t toRemove = cachedChunks_.size() - maxChunks;
    auto it = cachedChunks_.begin();
    for (size_t i = 0; i < toRemove && it != cachedChunks_.end(); ++i) {
        it = cachedChunks_.erase(it);
    }
}

std::string Client::getServerInfo() {
    if (!isConnected()) {
        return "Not connected";
    }
    
    try {
        auto result = rpcClient_->call("get_server_info");
        return result.as<std::string>();
    } catch (const std::exception& e) {
        handleRpcError(e);
        return "Error getting server info";
    }
}

bool Client::ping() {
    if (!rpcClient_) {
        return false;
    }
    
    try {
        auto result = rpcClient_->call("ping");
        return result.as<bool>();
    } catch (const std::exception& e) {
        return false;
    }
}

std::shared_ptr<ChunkSpan> Client::createChunkFromData(const AbsoluteChunkPosition& pos, const std::vector<uint8_t>& data) {
    auto overlayOpt = StatefulChunkOverlay::deserialize(data);
    if (!overlayOpt) {
        return nullptr;
    }
    
    return std::make_shared<SerializableChunkSpan>(pos, *overlayOpt);
}

std::vector<uint8_t> Client::serializeChunk(const ChunkSpan& chunk) {
    auto serializableChunk = dynamic_cast<const SerializableChunkSpan*>(&chunk);
    if (serializableChunk) {
        return serializableChunk->serialize();
    }
    
    // Fallback: create StatefulChunkOverlay from chunk data
    StatefulChunkOverlay overlay;
    for (uint32_t z = 0; z < CHUNK_DEPTH; ++z) {
        for (uint32_t y = 0; y < CHUNK_HEIGHT; ++y) {
            for (uint32_t x = 0; x < CHUNK_WIDTH; ++x) {
                const std::size_t idx = static_cast<std::size_t>(z) * chunk.strideZ + 
                                      static_cast<std::size_t>(y) * chunk.strideY + x;
                const Block block = chunk.data[idx];
                if (block != Block::Empty) {
                    overlay.setBlock(ChunkLocalPosition(x, y, z), block);
                }
            }
        }
    }
    return overlay.serialize();
}

void Client::cacheChunk(const AbsoluteChunkPosition& pos, std::shared_ptr<ChunkSpan> chunk) {
    cachedChunks_[pos] = chunk;
    
    // Auto-evict if cache gets too large
    evictOldChunks(100);
}

void Client::handleRpcError(const std::exception& e) {
    std::cerr << "RPC Error: " << e.what() << std::endl;
    // Could implement reconnection logic here
}