#include "client.h"
#include "chunkdims.h"
#include "chunkspan.h"
#include <iostream>
#include <algorithm>
#include <chrono>
#include <thread>
#include <future>
#include <chrono>

#include <grpcpp/grpcpp.h>

Client::Client(const std::string& host, uint16_t port, const std::string& playerId)
    : host_(host), port_(port), connected_(false), playerId_(playerId), playerPosition_{0, 0, 0} {
}

Client::~Client() {
    disconnect();
}

bool Client::connect() {
    try {
        std::string server_address = host_ + ":" + std::to_string(port_);
        channel_ = grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials());
        stub_ = blockserver::BlockServer::NewStub(channel_);
        std::cout << "Connected to server at " << server_address << std::endl;
        // Test connection with a ping
        connected_ = ping();
        
        if (connected_) {
            // Start background completion thread
            shouldStop_ = false;
            completionThread_ = std::thread(&Client::completionThreadFunc, this);
        }
        
        return connected_;
    } catch (const std::exception& e) {
        std::cerr << "Error connecting to server: " << e.what() << std::endl;
        handleRpcError(e);
        connected_ = false;
        return false;
    }
}

void Client::disconnect() {
    if (!stub_) {
        connected_ = false;
        return;
    }
    
    // First mark as disconnected to stop new requests
    connected_ = false;
    
    // Stop the completion thread by shutting down the completion queue first
    shouldStop_ = true;
    cq_.Shutdown();  // This will cause AsyncNext to return false and unblock the thread
    
    if (completionThread_.joinable()) {
        // Give it a reasonable timeout to join
        auto future = std::async(std::launch::async, [&]() {
            completionThread_.join();
        });
        
        if (future.wait_for(std::chrono::seconds(2)) == std::future_status::timeout) {
            std::cerr << "Warning: Completion thread did not join within timeout, detaching" << std::endl;
            completionThread_.detach();
        }
    }
    
    // Wait for pending requests to complete with timeout
    const auto timeout = std::chrono::seconds(5);
    const auto start = std::chrono::steady_clock::now();
    
    while (getPendingRequestCount() > 0) {
        // Check timeout
        if (std::chrono::steady_clock::now() - start > timeout) {
            std::cerr << "Warning: Disconnect timeout - " << getPendingRequestCount() 
                      << " requests may still be pending" << std::endl;
            break;
        }
        
        // Process any completed requests to clean up the list
        try {
            processPendingRequests();
        } catch (const std::exception& e) {
            // Ignore errors during shutdown
        }
        
        // Small delay to avoid busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Shutdown completion queue and clear remaining requests
    cq_.Shutdown();
    {
        std::lock_guard<std::mutex> lock(callsMutex_);
        pending_calls_.clear();
        requestBacklog_.clear();
    }
    
    {
        std::lock_guard<std::mutex> lock(requestedChunksMutex_);
        requestedChunks_.clear();
    }
    
    // Clear session token
    {
        std::lock_guard<std::mutex> lock(sessionMutex_);
        sessionToken_.clear();
    }
    
    // Now safe to destroy gRPC client
    stub_.reset();
    channel_.reset();
}

bool Client::isConnected() const {
    return connected_ && stub_ != nullptr;
}

void Client::setPlayerPosition(const AbsoluteBlockPosition& pos) {
    std::lock_guard<std::mutex> lock(playerMutex_);
    playerPosition_ = pos;
}

AbsoluteBlockPosition Client::getPlayerPosition() const {
    std::lock_guard<std::mutex> lock(playerMutex_);
    return playerPosition_;
}

std::string Client::getPlayerId() const {
    return playerId_;
}

bool Client::connectAsPlayer(const std::string& playerName, const AbsolutePrecisePosition& spawnPosition) {
    if (!isConnected()) {
        std::cerr << "Client not connected to server" << std::endl;
        return false;
    }
    
    try {
        blockserver::ConnectPlayerRequest request;
        request.set_player_name(playerName);
        request.set_spawn_x(spawnPosition.x);
        request.set_spawn_y(spawnPosition.y);
        request.set_spawn_z(spawnPosition.z);
        
        blockserver::ConnectPlayerResponse response;
        grpc::ClientContext context;
        
        grpc::Status status = stub_->ConnectPlayer(&context, request, &response);
        
        if (status.ok() && response.success()) {
            {
                std::lock_guard<std::mutex> lock(sessionMutex_);
                sessionToken_ = response.session_token();
            }
            
            playerId_ = response.player_id();
            
            // Update local position to actual spawn position
            AbsoluteBlockPosition spawnBlock{
                static_cast<int64_t>(response.actual_spawn_x()),
                static_cast<int64_t>(response.actual_spawn_y()),
                static_cast<int64_t>(response.actual_spawn_z())
            };
            setPlayerPosition(spawnBlock);
            
            std::cout << "Successfully connected as player: " << playerName 
                      << " at (" << response.actual_spawn_x() << ", " << response.actual_spawn_y() 
                      << ", " << response.actual_spawn_z() << ")" << std::endl;
            return true;
        } else {
            std::cerr << "Failed to connect as player: " << response.error_message() << std::endl;
            return false;
        }
        
    } catch (const std::exception& e) {
        handleRpcError(e);
        return false;
    }
}

bool Client::refreshSession() {
    if (!isConnected()) {
        return false;
    }
    
    std::string token;
    {
        std::lock_guard<std::mutex> lock(sessionMutex_);
        token = sessionToken_;
    }
    
    if (token.empty()) {
        return false;
    }
    
    try {
        blockserver::RefreshSessionRequest request;
        request.set_session_token(token);
        
        blockserver::RefreshSessionResponse response;
        grpc::ClientContext context;
        
        grpc::Status status = stub_->RefreshSession(&context, request, &response);
        
        if (status.ok() && response.success()) {
            return true;
        } else {
            std::cerr << "Failed to refresh session: " << response.error_message() << std::endl;
            // Clear invalid session
            {
                std::lock_guard<std::mutex> lock(sessionMutex_);
                sessionToken_.clear();
            }
            return false;
        }
        
    } catch (const std::exception& e) {
        handleRpcError(e);
        return false;
    }
}

bool Client::updatePlayerPosition(const AbsolutePrecisePosition& position) {
    if (!isConnected()) {
        return false;
    }
    
    std::string token;
    {
        std::lock_guard<std::mutex> lock(sessionMutex_);
        token = sessionToken_;
    }
    
    if (token.empty()) {
        std::cerr << "No valid session token for position update" << std::endl;
        return false;
    }
    
    try {
        blockserver::UpdatePlayerPositionRequest request;
        request.set_session_token(token);
        request.set_x(position.x);
        request.set_y(position.y);
        request.set_z(position.z);
        
        blockserver::UpdatePlayerPositionResponse response;
        grpc::ClientContext context;
        
        grpc::Status status = stub_->UpdatePlayerPosition(&context, request, &response);
        
        if (status.ok() && response.success()) {
            // Update local position as well
            AbsoluteBlockPosition blockPos = toAbsoluteBlock(position);
            setPlayerPosition(blockPos);
            return true;
        } else {
            std::cerr << "Failed to update player position: " << response.error_message() << std::endl;
            if (response.error_message().find("Invalid or expired session") != std::string::npos) {
                // Clear invalid session
                std::lock_guard<std::mutex> lock(sessionMutex_);
                sessionToken_.clear();
            }
            return false;
        }
        
    } catch (const std::exception& e) {
        handleRpcError(e);
        return false;
    }
}

bool Client::disconnectPlayer() {
    if (!isConnected()) {
        return false;
    }
    
    std::string token;
    {
        std::lock_guard<std::mutex> lock(sessionMutex_);
        token = sessionToken_;
    }
    
    if (token.empty()) {
        return true; // Already disconnected
    }
    
    try {
        blockserver::DisconnectPlayerRequest request;
        request.set_session_token(token);
        
        blockserver::DisconnectPlayerResponse response;
        grpc::ClientContext context;
        
        grpc::Status status = stub_->DisconnectPlayer(&context, request, &response);
        
        {
            std::lock_guard<std::mutex> lock(sessionMutex_);
            sessionToken_.clear();
        }
        
        if (status.ok() && response.success()) {
            std::cout << "Successfully disconnected player" << std::endl;
            return true;
        } else {
            std::cerr << "Failed to disconnect player: " << response.error_message() << std::endl;
            return false;
        }
        
    } catch (const std::exception& e) {
        handleRpcError(e);
        {
            std::lock_guard<std::mutex> lock(sessionMutex_);
            sessionToken_.clear();
        }
        return false;
    }
}

bool Client::hasValidSession() const {
    std::lock_guard<std::mutex> lock(sessionMutex_);
    return !sessionToken_.empty();
}

std::string Client::getSessionToken() const {
    std::lock_guard<std::mutex> lock(sessionMutex_);
    return sessionToken_;
}

std::optional<std::shared_ptr<ChunkSpan>> Client::requestChunk(const AbsoluteChunkPosition& pos) {
    // Check cache first
    auto cached = getCachedChunk(pos);
    if (cached) {
        return cached;
    }
    
    // Start async request if not already requested
    requestChunkAsync(pos);
    
    // Return nothing - chunk will be available later
    return std::nullopt;
}

void Client::requestChunkAsync(const AbsoluteChunkPosition& pos) {
    if (!isConnected()) {
        std::cerr << "Client not connected" << std::endl;
        return;
    }
    
    // Check if already requested
    {
        std::lock_guard<std::mutex> lock(requestedChunksMutex_);
        if (requestedChunks_.find(pos) != requestedChunks_.end()) {
            return; // Already requested
        }
        requestedChunks_.insert(pos);
    }
    
    // Respect in-flight limit; if saturated, push to backlog to send later
    {
        std::lock_guard<std::mutex> lock(callsMutex_);
        if (pending_calls_.size() >= kMaxInflightRequests) {
            requestBacklog_.push_back(pos);
            return;
        }
    }

    try {
        // Create async call structure
        auto call = std::make_unique<AsyncChunkCall>();
        call->position = pos;
        *call->request.mutable_player_position() = createPlayerPositionMessage();
        call->request.set_x(pos.x);
        call->request.set_y(pos.y);
        call->request.set_z(pos.z);
        call->request_time = std::chrono::steady_clock::now();
        
        // Start async call
        call->response_reader = stub_->AsyncGetChunk(&call->context, call->request, &cq_);
        
        // Use the call pointer as tag for completion queue
        void* tag = call.get();
        call->response_reader->Finish(&call->response, &call->status, tag);
        
        // Store call for later processing
        {
            std::lock_guard<std::mutex> lock(callsMutex_);
            pending_calls_[tag] = std::move(call);
        }

    } catch (const std::exception& e) {
        handleRpcError(e);
        // Remove from requested chunks on error
        std::lock_guard<std::mutex> lock(requestedChunksMutex_);
        requestedChunks_.erase(pos);
    }
}

void Client::processPendingRequests() {
    // Early exit if not connected to avoid processing during shutdown
    if (!isConnected() && stub_ == nullptr) {
        // If we're in shutdown mode, still clean up pending calls
        std::lock_guard<std::mutex> lock(callsMutex_);
        pending_calls_.clear();
        return;
    }
    
    void* tag;
    bool ok;
    
    // Process all ready completions (non-blocking)
    while (cq_.AsyncNext(&tag, &ok, std::chrono::system_clock::now() + std::chrono::milliseconds(10)) == grpc::CompletionQueue::GOT_EVENT) {
        std::unique_ptr<AsyncChunkCall> call;
        
        {
            std::lock_guard<std::mutex> lock(callsMutex_);
            auto it = pending_calls_.find(tag);
            if (it != pending_calls_.end()) {
                call = std::move(it->second);
                pending_calls_.erase(it);
            }
        }
        
        if (!call) continue;
        
        try {
            if (ok && call->status.ok() && call->response.success()) {
                if (call->response.has_chunk_data()) {
                    const std::string& chunkDataStr = call->response.chunk_data();
                    std::vector<uint8_t> serializedData(chunkDataStr.begin(), chunkDataStr.end());
                    
                    std::cout << "Got response for chunk (" << call->position.x << ", " 
                              << call->position.y << ", " << call->position.z 
                              << ") - Data size: " << serializedData.size() << " bytes" << std::endl;
                    
                    // Deserialize the chunk directly
                    try {
                        auto chunk = std::make_shared<ChunkSpan>(serializedData);
                        cacheChunk(call->position, chunk);
                        
                        std::cout << "Loaded chunk (" << call->position.x << ", " 
                                  << call->position.y << ", " << call->position.z << ")" << std::endl;
                    } catch (const std::exception& e) {
                        std::cerr << "Failed to deserialize chunk data for position (" 
                                  << call->position.x << ", " << call->position.y << ", " 
                                  << call->position.z << "): " << e.what() << std::endl;
                    }
                }
            } else {
                std::cerr << "gRPC error for chunk (" << call->position.x << ", " 
                          << call->position.y << ", " << call->position.z << "): ";
                if (!call->status.ok()) {
                    std::cerr << call->status.error_message();
                } else if (!ok) {
                    std::cerr << "Completion queue error";
                } else {
                    std::cerr << call->response.error_message();
                }
                std::cerr << std::endl;
            }
        } catch (const std::exception& e) {
            handleRpcError(e);
        }
        
        // Remove from requested chunks
        {
            std::lock_guard<std::mutex> requestedLock(requestedChunksMutex_);
            requestedChunks_.erase(call->position);
        }
        
        // Drain backlog up to capacity
        {
            std::lock_guard<std::mutex> lock(callsMutex_);
            while (!requestBacklog_.empty() && pending_calls_.size() < kMaxInflightRequests) {
                AbsoluteChunkPosition next = requestBacklog_.front();
                requestBacklog_.pop_front();
                
                // Double-check it wasn't satisfied while in backlog
                {
                    std::lock_guard<std::mutex> cacheLock(cacheMutex_);
                    if (cachedChunks_.find(next) != cachedChunks_.end()) {
                        std::lock_guard<std::mutex> requestedLock(requestedChunksMutex_);
                        requestedChunks_.erase(next);
                        continue;
                    }
                }
                
                try {
                    // Create new async call for backlog item
                    auto backlogCall = std::make_unique<AsyncChunkCall>();
                    backlogCall->position = next;
                    *backlogCall->request.mutable_player_position() = createPlayerPositionMessage();
                    backlogCall->request.set_x(next.x);
                    backlogCall->request.set_y(next.y);
                    backlogCall->request.set_z(next.z);
                    backlogCall->request_time = std::chrono::steady_clock::now();
                    
                    backlogCall->response_reader = stub_->AsyncGetChunk(&backlogCall->context, backlogCall->request, &cq_);
                    
                    void* backlogTag = backlogCall.get();
                    backlogCall->response_reader->Finish(&backlogCall->response, &backlogCall->status, backlogTag);
                    
                    pending_calls_[backlogTag] = std::move(backlogCall);
                    
                } catch (const std::exception& e) {
                    handleRpcError(e);
                    std::lock_guard<std::mutex> requestedLock(requestedChunksMutex_);
                    requestedChunks_.erase(next);
                }
            }
        }
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
        blockserver::PlaceBlockRequest request;
        *request.mutable_player_position() = createPlayerPositionMessage();
        request.set_x(pos.x);
        request.set_y(pos.y);
        request.set_z(pos.z);
        request.set_block_type(static_cast<uint32_t>(block));
        
        blockserver::PlaceBlockResponse response;
        grpc::ClientContext context;
        
        auto status = stub_->PlaceBlock(&context, request, &response);
        
        if (status.ok() && response.success()) {
            // Update local cache if chunk is loaded
            auto chunkPos = toAbsoluteChunk(pos);
            auto cached = getCachedChunk(chunkPos);
            if (cached) {
                auto localPos = toChunkLocal(pos, chunkPos);
                (*cached)->setBlock(localPos, block);
            }
            return true;
        } else {
            std::cerr << "PlaceBlock failed: ";
            if (!status.ok()) {
                std::cerr << status.error_message();
            } else {
                std::cerr << response.error_message();
            }
            std::cerr << std::endl;
            return false;
        }
    } catch (const std::exception& e) {
        handleRpcError(e);
        return false;
    }
}

bool Client::breakBlock(const AbsoluteBlockPosition& pos) {
    return placeBlock(pos, Block::Empty);
}

std::vector<AbsoluteChunkPosition> Client::getUpdatedChunks(int32_t renderDistance) {
    if (!isConnected()) {
        std::cerr << "Client not connected" << std::endl;
        return {};
    }
    
    try {
        blockserver::UpdatedChunksRequest request;
        *request.mutable_player_position() = createPlayerPositionMessage();
        request.set_render_distance(renderDistance);
        
        blockserver::UpdatedChunksResponse response;
        grpc::ClientContext context;
        
        auto status = stub_->GetUpdatedChunks(&context, request, &response);
        
        if (status.ok() && response.success()) {
            std::vector<AbsoluteChunkPosition> updatedChunks;
            for (const auto& chunk : response.updated_chunks()) {
                updatedChunks.push_back({chunk.x(), chunk.y(), chunk.z()});
            }
            return updatedChunks;
        } else {
            std::cerr << "Failed to get updated chunks: " << response.error_message() << std::endl;
            return {};
        }
    } catch (const std::exception& e) {
        handleRpcError(e);
        return {};
    }
}

std::optional<Block> Client::getBlockAt(const AbsoluteBlockPosition& pos) {
    // First check local cache
    auto chunkPos = toAbsoluteChunk(pos);
    auto cached = getCachedChunk(chunkPos);
    if (cached) {
        auto localPos = toChunkLocal(pos, chunkPos);
        return (*cached)->getBlock(localPos);
    }
    
    // If not in cache, request the chunk
    auto chunk = requestChunk(chunkPos);
    if (chunk) {
        auto localPos = toChunkLocal(pos, chunkPos);
        return (*chunk)->getBlock(localPos);
    }
    
    return std::nullopt;
}

void Client::clearCache() {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    cachedChunks_.clear();
}

std::optional<std::shared_ptr<ChunkSpan>> Client::getCachedChunk(const AbsoluteChunkPosition& pos) const {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    auto it = cachedChunks_.find(pos);
    if (it != cachedChunks_.end()) {
        return it->second;
    }
    return std::nullopt;
}

size_t Client::getCacheSize() const {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    return cachedChunks_.size();
}

void Client::evictOldChunks(size_t maxChunks) {
    std::lock_guard<std::mutex> lock(cacheMutex_);
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
        blockserver::ServerInfoRequest request;
        blockserver::ServerInfoResponse response;
        grpc::ClientContext context;
        
        auto status = stub_->GetServerInfo(&context, request, &response);
        
        if (status.ok() && response.success()) {
            return response.server_info();
        } else {
            std::string error = "Error getting server info: ";
            if (!status.ok()) {
                error += status.error_message();
            } else {
                error += response.error_message();
            }
            return error;
        }
    } catch (const std::exception& e) {
        handleRpcError(e);
        return "Error getting server info";
    }
}

bool Client::ping() {
    if (!stub_) {
        std::cerr << "no stub_" << std::endl;
        return false;
    }
    
    try {
        blockserver::PingRequest request;
        blockserver::PingResponse response;
        grpc::ClientContext context;
        
        auto status = stub_->Ping(&context, request, &response);
        std::cout << "Ping status: " << (status.ok() ? "OK" : "Failed") << std::endl;
        std::cout << "Ping response success: " << (response.success() ? "Yes" : "No") << std::endl;
        return status.ok() && response.success();
    } catch (const std::exception& e) {
        return false;
    }
}

std::shared_ptr<ChunkSpan> Client::createChunkFromData(const AbsoluteChunkPosition& pos, const std::vector<uint8_t>& data) {
    try {
        auto serializedData = const_cast<std::vector<uint8_t>&>(data); // ChunkSpan constructor may modify the vector
        return std::make_shared<ChunkSpan>(serializedData);
    } catch (const std::exception& e) {
        std::cerr << "Failed to create chunk from data: " << e.what() << std::endl;
        return nullptr;
    }
}

std::vector<uint8_t> Client::serializeChunk(const ChunkSpan& chunk) {
    return chunk.serialize();
}

void Client::cacheChunk(const AbsoluteChunkPosition& pos, std::shared_ptr<ChunkSpan> chunk) {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    cachedChunks_[pos] = chunk;
    
    // Auto-evict if cache gets too large
    if (cachedChunks_.size() > 100) {
        // Simple eviction: remove excess chunks
        size_t toRemove = cachedChunks_.size() - 100;
        auto it = cachedChunks_.begin();
        for (size_t i = 0; i < toRemove && it != cachedChunks_.end(); ++i) {
            it = cachedChunks_.erase(it);
        }
    }
}

void Client::handleRpcError(const std::exception& e) {
    std::cerr << "RPC Error: " << e.what() << std::endl;
    // Could implement reconnection logic here
}

blockserver::PlayerPosition Client::createPlayerPositionMessage() const {
    blockserver::PlayerPosition playerPos;
    playerPos.set_player_id(playerId_);
    
    std::lock_guard<std::mutex> lock(playerMutex_);
    playerPos.set_x(playerPosition_.x);
    playerPos.set_y(playerPosition_.y);
    playerPos.set_z(playerPosition_.z);
    
    return playerPos;
}

size_t Client::getPendingRequestCount() const {
    std::lock_guard<std::mutex> lock(callsMutex_);
    return pending_calls_.size();
}

void Client::completionThreadFunc() {
    while (!shouldStop_) {
        try {
            // Use a timeout for AsyncNext to make it more responsive to shutdown
            void* tag;
            bool ok;
            
            // Check completion queue with a small timeout instead of blocking indefinitely
            auto deadline = std::chrono::system_clock::now() + std::chrono::milliseconds(100);
            grpc::CompletionQueue::NextStatus status = cq_.AsyncNext(&tag, &ok, deadline);
            
            if (status == grpc::CompletionQueue::NextStatus::GOT_EVENT) {
                // Process the event
                std::unique_ptr<AsyncChunkCall> call;
                
                {
                    std::lock_guard<std::mutex> lock(callsMutex_);
                    auto it = pending_calls_.find(tag);
                    if (it != pending_calls_.end()) {
                        call = std::move(it->second);
                        pending_calls_.erase(it);
                    }
                }
                
                if (call) {
                    // Process the completed call (same logic as in processPendingRequests)
                    try {
                        if (ok && call->status.ok() && call->response.success()) {
                            if (call->response.has_chunk_data()) {
                                const std::string& chunkDataStr = call->response.chunk_data();
                                std::vector<uint8_t> serializedData(chunkDataStr.begin(), chunkDataStr.end());
                                
                                std::cout << "Got response for chunk (" << call->position.x << ", " 
                                          << call->position.y << ", " << call->position.z 
                                          << ") - Data size: " << serializedData.size() << " bytes" << std::endl;
                                
                                try {
                                    auto chunk = std::make_shared<ChunkSpan>(serializedData);
                                    cacheChunk(call->position, chunk);
                                    
                                    std::cout << "Loaded chunk (" << call->position.x << ", " 
                                              << call->position.y << ", " << call->position.z << ")" << std::endl;
                                } catch (const std::exception& e) {
                                    std::cerr << "Failed to deserialize chunk data for position (" 
                                              << call->position.x << ", " << call->position.y << ", " 
                                              << call->position.z << "): " << e.what() << std::endl;
                                }
                            }
                        } else {
                            std::cerr << "gRPC error for chunk (" << call->position.x << ", " 
                                      << call->position.y << ", " << call->position.z << "): ";
                            if (!call->status.ok()) {
                                std::cerr << call->status.error_message();
                            } else if (!ok) {
                                std::cerr << "Completion queue error";
                            } else {
                                std::cerr << call->response.error_message();
                            }
                            std::cerr << std::endl;
                        }
                    } catch (const std::exception& e) {
                        handleRpcError(e);
                    }
                    
                    // Remove from requested chunks
                    {
                        std::lock_guard<std::mutex> requestedLock(requestedChunksMutex_);
                        requestedChunks_.erase(call->position);
                    }
                    
                    // Process backlog if not shutting down
                    if (!shouldStop_) {
                        std::lock_guard<std::mutex> lock(callsMutex_);
                        while (!requestBacklog_.empty() && pending_calls_.size() < kMaxInflightRequests) {
                            AbsoluteChunkPosition next = requestBacklog_.front();
                            requestBacklog_.pop_front();
                            
                            // Double-check it wasn't satisfied while in backlog
                            {
                                std::lock_guard<std::mutex> cacheLock(cacheMutex_);
                                if (cachedChunks_.find(next) != cachedChunks_.end()) {
                                    std::lock_guard<std::mutex> requestedLock(requestedChunksMutex_);
                                    requestedChunks_.erase(next);
                                    continue;
                                }
                            }
                            
                            try {
                                // Create new async call for backlog item
                                auto backlogCall = std::make_unique<AsyncChunkCall>();
                                backlogCall->position = next;
                                *backlogCall->request.mutable_player_position() = createPlayerPositionMessage();
                                backlogCall->request.set_x(next.x);
                                backlogCall->request.set_y(next.y);
                                backlogCall->request.set_z(next.z);
                                backlogCall->request_time = std::chrono::steady_clock::now();
                                
                                backlogCall->response_reader = stub_->AsyncGetChunk(&backlogCall->context, backlogCall->request, &cq_);
                                
                                void* backlogTag = backlogCall.get();
                                backlogCall->response_reader->Finish(&backlogCall->response, &backlogCall->status, backlogTag);
                                
                                pending_calls_[backlogTag] = std::move(backlogCall);
                                
                            } catch (const std::exception& e) {
                                handleRpcError(e);
                                std::lock_guard<std::mutex> requestedLock(requestedChunksMutex_);
                                requestedChunks_.erase(next);
                            }
                        }
                    }
                }
            } else if (status == grpc::CompletionQueue::NextStatus::SHUTDOWN) {
                // Completion queue was shut down, exit the loop
                break;
            }
            // If TIMEOUT, just continue the loop and check shouldStop_
            
        } catch (const std::exception& e) {
            // Log error but continue processing
            std::cerr << "Error in completion thread: " << e.what() << std::endl;
            // Small delay to avoid tight error loops
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}