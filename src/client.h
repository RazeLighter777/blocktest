#pragma once

#include <memory>
#include <optional>
#include <unordered_map>
#include <string>
#include <vector>
#include <future>
#include <thread>
#include <queue>
#include <deque>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <chrono>
#include <unordered_set>
#include <grpcpp/grpcpp.h>
#include "blockserver.grpc.pb.h"
#include "chunkoverlay.h"
#include "position.h"
#include "block.h"
#include "world.h"

// Client-side chunk cache
using ClientChunkMap = std::unordered_map<AbsoluteChunkPosition, std::shared_ptr<ChunkSpan>, ChunkPosHash, ChunkPosEq>;

// Async chunk request tracking
struct ChunkRequest {
    AbsoluteChunkPosition position;
    std::future<grpc::Status> future;
    std::shared_ptr<blockserver::ChunkResponse> response;
    std::chrono::steady_clock::time_point requestTime;
};

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
    
    // Chunk operations (non-blocking)
    std::optional<std::shared_ptr<ChunkSpan>> requestChunk(const AbsoluteChunkPosition& pos);
    void requestChunkAsync(const AbsoluteChunkPosition& pos);
    void preloadChunksAroundPosition(const AbsoluteBlockPosition& position, size_t radiusInChunks = 5);
    
    // Process pending async requests (call this regularly from render thread)
    void processPendingRequests();
    
    // Get number of pending requests (for shutdown coordination)
    size_t getPendingRequestCount() const;
    
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
    // Limit concurrent in-flight chunk RPCs to avoid flooding server
    static constexpr std::size_t kMaxInflightRequests = 64;
    
    // Network connection
    std::unique_ptr<blockserver::BlockServer::Stub> stub_;
    std::shared_ptr<grpc::Channel> channel_;
    std::string host_;
    uint16_t port_;
    std::atomic<bool> connected_;
    
    // Local chunk cache
    ClientChunkMap cachedChunks_;
    mutable std::mutex cacheMutex_;
    
    // Async request tracking
    std::vector<ChunkRequest> pendingRequests_;
    mutable std::mutex requestsMutex_;
    // Backlog of requested chunks waiting to be sent when capacity allows
    std::deque<AbsoluteChunkPosition> requestBacklog_;
    std::unordered_set<AbsoluteChunkPosition, ChunkPosHash, ChunkPosEq> requestedChunks_;
    std::mutex requestedChunksMutex_;
    
    // Helper methods
    std::shared_ptr<ChunkSpan> createChunkFromData(const AbsoluteChunkPosition& pos, const std::vector<uint8_t>& data);
    std::vector<uint8_t> serializeChunk(const ChunkSpan& chunk);
    void cacheChunk(const AbsoluteChunkPosition& pos, std::shared_ptr<ChunkSpan> chunk);
    
    // Error handling
    void handleRpcError(const std::exception& e);
};