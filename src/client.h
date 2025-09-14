#pragma once

#include <memory>
#include <optional>
#include <unordered_map>
#include <string>
#include <vector>
#include "rpc/client.h"
#include "chunkoverlay.h"
#include "position.h"
#include "block.h"
#include "world.h"

// Client-side chunk cache
using ClientChunkMap = std::unordered_map<AbsoluteChunkPosition, std::shared_ptr<ChunkSpan>, ChunkPosHash, ChunkPosEq>;

class Client {
public:
    // Constructor
    Client(const std::string& host = "127.0.0.1", uint16_t port = 8080);
    
    // Destructor
    ~Client();
    
    // Connection management
    bool connect();
    void disconnect();
    bool isConnected() const;
    
    // Chunk operations
    std::optional<std::shared_ptr<ChunkSpan>> requestChunk(const AbsoluteChunkPosition& pos);
    void preloadChunksAroundPosition(const AbsoluteBlockPosition& position, size_t radiusInChunks = 5);
    
    // Block operations
    bool placeBlock(const AbsoluteBlockPosition& pos, Block block);
    bool breakBlock(const AbsoluteBlockPosition& pos);
    std::optional<Block> getBlockAt(const AbsoluteBlockPosition& pos);
    
    // Cache management
    void clearCache();
    std::optional<std::shared_ptr<ChunkSpan>> getCachedChunk(const AbsoluteChunkPosition& pos) const;
    size_t getCacheSize() const;
    void evictOldChunks(size_t maxChunks = 100);
    
    // Server information
    std::string getServerInfo();
    bool ping();

private:
    // Network connection
    std::unique_ptr<rpc::client> rpcClient_;
    std::string host_;
    uint16_t port_;
    bool connected_;
    
    // Local chunk cache
    ClientChunkMap cachedChunks_;
    
    // Helper methods
    std::shared_ptr<ChunkSpan> createChunkFromData(const AbsoluteChunkPosition& pos, const std::vector<uint8_t>& data);
    std::vector<uint8_t> serializeChunk(const ChunkSpan& chunk);
    void cacheChunk(const AbsoluteChunkPosition& pos, std::shared_ptr<ChunkSpan> chunk);
    
    // Error handling
    void handleRpcError(const std::exception& e);
};