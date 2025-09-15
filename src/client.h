#pragma once

#include <memory>
#include <optional>
#include <unordered_map>
#include <string>
#include <vector>
#include <deque>
#include <mutex>
#include <atomic>
#include <chrono>
#include <thread>
#include <unordered_set>
#include <grpcpp/grpcpp.h>
#include "blockserver.grpc.pb.h"
#include "chunktransform.h"
#include "position.h"
#include "block.h"
#include "world.h"

// Client-side chunk cache
using ClientChunkMap = std::unordered_map<AbsoluteChunkPosition, std::shared_ptr<ChunkSpan>, ChunkPosHash, ChunkPosEq>;

// Async chunk request tracking
struct AsyncChunkCall {
    AbsoluteChunkPosition position;
    blockserver::ChunkRequest request;
    blockserver::ChunkResponse response;
    grpc::ClientContext context;
    grpc::Status status;
    std::unique_ptr<grpc::ClientAsyncResponseReader<blockserver::ChunkResponse>> response_reader;
    std::chrono::steady_clock::time_point request_time;
};

class Client {
public:
    // Constructor
    Client(const std::string& host = "127.0.0.1", uint16_t port = 8080, const std::string& playerId = "player1");
    
    // Destructor
    ~Client();
    
    // Connection management
    bool connect();
    void disconnect();
    bool isConnected() const;
    
    // Player management
    void setPlayerPosition(const AbsoluteBlockPosition& pos);
    AbsoluteBlockPosition getPlayerPosition() const;
    std::string getPlayerId() const;
    
    // Player session management
    bool connectAsPlayer(const std::string& playerName, const AbsolutePrecisePosition& spawnPosition);
    bool refreshSession();
    bool updatePlayerPosition(const AbsolutePrecisePosition& position);
    bool disconnectPlayer();
    bool hasValidSession() const;
    std::string getSessionToken() const;
    
    // Chunk operations (non-blocking)
    std::optional<std::shared_ptr<ChunkSpan>> requestChunk(const AbsoluteChunkPosition& pos);
    void requestChunkAsync(const AbsoluteChunkPosition& pos);
    void preloadChunksAroundPosition(const AbsoluteBlockPosition& position, size_t radiusInChunks = 5);
    
    // Updated chunks tracking
    std::vector<AbsoluteChunkPosition> getUpdatedChunks(int32_t renderDistance = 5);
    
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
    grpc::CompletionQueue cq_;
    std::string host_;
    uint16_t port_;
    std::atomic<bool> connected_;
    
    // Player information
    std::string playerId_;
    AbsoluteBlockPosition playerPosition_;
    mutable std::mutex playerMutex_;
    
    // Session management
    std::string sessionToken_;
    mutable std::mutex sessionMutex_;
    
    // Local chunk cache
    ClientChunkMap cachedChunks_;
    mutable std::mutex cacheMutex_;
    
    // Async request tracking
    std::unordered_map<void*, std::unique_ptr<AsyncChunkCall>> pending_calls_;
    mutable std::mutex callsMutex_;
    // Backlog of requested chunks waiting to be sent when capacity allows
    std::deque<AbsoluteChunkPosition> requestBacklog_;
    std::unordered_set<AbsoluteChunkPosition, ChunkPosHash, ChunkPosEq> requestedChunks_;
    std::mutex requestedChunksMutex_;
    
    // Background completion queue processing
    std::thread completionThread_;
    std::atomic<bool> shouldStop_{false};
    
    // Helper methods
    std::shared_ptr<ChunkSpan> createChunkFromData(const AbsoluteChunkPosition& pos, const std::vector<uint8_t>& data);
    std::vector<uint8_t> serializeChunk(const ChunkSpan& chunk);
    void cacheChunk(const AbsoluteChunkPosition& pos, std::shared_ptr<ChunkSpan> chunk);
    blockserver::PlayerPosition createPlayerPositionMessage() const;
    void completionThreadFunc();
    
    // Error handling
    void handleRpcError(const std::exception& e);
};