#include "client.h"
#include "statefulchunkoverlay.h"
#include "chunkdims.h"
#include <iostream>
#include <algorithm>
#include <chrono>
#include <thread>
#include <grpcpp/grpcpp.h>

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
        std::string server_address = host_ + ":" + std::to_string(port_);
        channel_ = grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials());
        stub_ = blockserver::BlockServer::NewStub(channel_);
        
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
    if (!stub_) {
        connected_ = false;
        return;
    }
    
    // First mark as disconnected to stop new requests
    connected_ = false;
    
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
    
    // Clear any remaining requests
    {
        std::lock_guard<std::mutex> lock(requestsMutex_);
        pendingRequests_.clear();
        requestBacklog_.clear();
    }
    
    {
        std::lock_guard<std::mutex> lock(requestedChunksMutex_);
        requestedChunks_.clear();
    }
    
    // Now safe to destroy gRPC client
    stub_.reset();
    channel_.reset();
}

bool Client::isConnected() const {
    return connected_ && stub_ != nullptr;
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
        std::lock_guard<std::mutex> lock(requestsMutex_);
        if (pendingRequests_.size() >= kMaxInflightRequests) {
            requestBacklog_.push_back(pos);
            return;
        }
    }

    try {
        // Prepare request and response
        auto request = std::make_shared<blockserver::ChunkRequest>();
        request->set_x(pos.x);
        request->set_y(pos.y);
        request->set_z(pos.z);
        
        auto response = std::make_shared<blockserver::ChunkResponse>();
        auto context = std::make_shared<grpc::ClientContext>();
        
        // Start async call
        auto future = std::async(std::launch::async, [this, context, request, response]() {
            return stub_->GetChunk(context.get(), *request, response.get());
        });

        ChunkRequest chunkReq;
        chunkReq.position = pos;
        chunkReq.future = std::move(future);
        chunkReq.response = response;
        chunkReq.requestTime = std::chrono::steady_clock::now();

        std::lock_guard<std::mutex> lock(requestsMutex_);
        pendingRequests_.push_back(std::move(chunkReq));

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
        // If we're in shutdown mode, still process ready futures to clean up
        std::lock_guard<std::mutex> lock(requestsMutex_);
        auto it = pendingRequests_.begin();
        while (it != pendingRequests_.end()) {
            auto& request = *it;
            auto status = request.future.wait_for(std::chrono::milliseconds(0));
            if (status == std::future_status::ready) {
                try {
                    // Just consume the result to clean up the future
                    request.future.get();
                } catch (...) {
                    // Ignore all errors during shutdown
                }
                it = pendingRequests_.erase(it);
            } else {
                ++it;
            }
        }
        return;
    }
    
    std::lock_guard<std::mutex> lock(requestsMutex_);
    
    static int debugCallCounter = 0;
    debugCallCounter++;
    
    // Debug: Print pending requests count periodically
    if (debugCallCounter % 600 == 0) {
        std::cout << "Processing pending requests... Count: " << pendingRequests_.size() << std::endl;
    }
    
    auto it = pendingRequests_.begin();
    while (it != pendingRequests_.end()) {
        auto& request = *it;
        
        // Check if future is ready
        auto status = request.future.wait_for(std::chrono::milliseconds(0));
        if (status == std::future_status::ready) {
            try {
                auto grpcStatus = request.future.get();
                
                if (grpcStatus.ok() && request.response->success()) {
                    const std::string& chunkDataStr = request.response->chunk_data();
                    std::vector<uint8_t> serializedData(chunkDataStr.begin(), chunkDataStr.end());
                    
                    std::cout << "Got response for chunk (" << request.position.x << ", " 
                              << request.position.y << ", " << request.position.z 
                              << ") - Data size: " << serializedData.size() << " bytes" << std::endl;
                    
                    // Deserialize the overlay
                    auto overlayOpt = StatefulChunkOverlay::deserialize(serializedData);
                    if (overlayOpt) {
                        // Create chunk from overlay
                        auto chunk = std::make_shared<SerializableChunkSpan>(request.position, *overlayOpt);
                        
                        // Cache the chunk
                        cacheChunk(request.position, chunk);
                        
                        std::cout << "Loaded chunk (" << request.position.x << ", " 
                                  << request.position.y << ", " << request.position.z << ")" << std::endl;
                    } else {
                        std::cerr << "Failed to deserialize chunk data for position (" 
                                  << request.position.x << ", " << request.position.y << ", " 
                                  << request.position.z << ")" << std::endl;
                    }
                } else {
                    std::cerr << "gRPC error for chunk (" << request.position.x << ", " 
                              << request.position.y << ", " << request.position.z << "): ";
                    if (!grpcStatus.ok()) {
                        std::cerr << grpcStatus.error_message();
                    } else {
                        std::cerr << request.response->error_message();
                    }
                    std::cerr << std::endl;
                }
                
                // Remove from requested chunks
                {
                    std::lock_guard<std::mutex> requestedLock(requestedChunksMutex_);
                    requestedChunks_.erase(request.position);
                }
                
                // Remove from pending requests
                it = pendingRequests_.erase(it);
                
            } catch (const std::exception& e) {
                handleRpcError(e);
                
                // Remove from requested chunks on error
                {
                    std::lock_guard<std::mutex> requestedLock(requestedChunksMutex_);
                    requestedChunks_.erase(request.position);
                }
                
                // Remove from pending requests
                it = pendingRequests_.erase(it);
            }
        } else {
            // Check for timeout (optional)
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - request.requestTime);
            if (elapsed.count() > 30) { // 30 second timeout
                std::cerr << "Chunk request timeout for position (" 
                          << request.position.x << ", " << request.position.y << ", " 
                          << request.position.z << ")" << std::endl;
                
                // Remove from requested chunks on timeout
                {
                    std::lock_guard<std::mutex> requestedLock(requestedChunksMutex_);
                    requestedChunks_.erase(request.position);
                }
                
                // Remove from pending requests
                it = pendingRequests_.erase(it);
            } else {
                // Still waiting - check status for debugging
                if (debugCallCounter % 300 == 0 && !pendingRequests_.empty()) {
                    std::cout << "Waiting for chunk (" << request.position.x << ", " 
                              << request.position.y << ", " << request.position.z 
                              << ") - Elapsed: " << elapsed.count() << "s" << std::endl;
                }
                ++it;
            }
        }

        // Drain backlog up to capacity after processing current futures
        while (!requestBacklog_.empty() && pendingRequests_.size() < kMaxInflightRequests) {
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
                // Prepare request and response
                auto grpcRequest = std::make_shared<blockserver::ChunkRequest>();
                grpcRequest->set_x(next.x);
                grpcRequest->set_y(next.y);
                grpcRequest->set_z(next.z);
                
                auto response = std::make_shared<blockserver::ChunkResponse>();
                auto context = std::make_shared<grpc::ClientContext>();
                
                // Start async call
                auto future = std::async(std::launch::async, [this, context, grpcRequest, response]() {
                    return stub_->GetChunk(context.get(), *grpcRequest, response.get());
                });
                
                ChunkRequest req;
                req.position = next;
                req.future = std::move(future);
                req.response = response;
                req.requestTime = std::chrono::steady_clock::now();
                pendingRequests_.push_back(std::move(req));
            } catch (const std::exception& e) {
                handleRpcError(e);
                std::lock_guard<std::mutex> requestedLock(requestedChunksMutex_);
                requestedChunks_.erase(next);
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
                const std::size_t idx = static_cast<std::size_t>(localPos.z) * (*cached)->strideZ + 
                                      static_cast<std::size_t>(localPos.y) * (*cached)->strideY + localPos.x;
                (*cached)->data[idx] = block;
                
                // Update overlay if it's a SerializableChunkSpan
                auto serializableChunk = std::dynamic_pointer_cast<SerializableChunkSpan>(*cached);
                if (serializableChunk) {
                    serializableChunk->updateOverlay();
                }
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

std::optional<Block> Client::getBlockAt(const AbsoluteBlockPosition& pos) {
    // First check local cache
    auto chunkPos = toAbsoluteChunk(pos);
    auto cached = getCachedChunk(chunkPos);
    if (cached) {
        auto localPos = toChunkLocal(pos, chunkPos);
        const std::size_t idx = static_cast<std::size_t>(localPos.z) * (*cached)->strideZ + 
                              static_cast<std::size_t>(localPos.y) * (*cached)->strideY + localPos.x;
        return (*cached)->data[idx];
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
        return false;
    }
    
    try {
        blockserver::PingRequest request;
        blockserver::PingResponse response;
        grpc::ClientContext context;
        
        auto status = stub_->Ping(&context, request, &response);
        return status.ok() && response.success();
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
    // Check if it's a SerializableChunkSpan by trying to cast the pointer
    const SerializableChunkSpan* serializableChunk = dynamic_cast<const SerializableChunkSpan*>(&chunk);
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

size_t Client::getPendingRequestCount() const {
    std::lock_guard<std::mutex> lock(requestsMutex_);
    return pendingRequests_.size();
}