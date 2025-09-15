#include "server.h"
#include "chunkdims.h"
#include <iostream>
#include <stdexcept>
#include <thread>
#include <grpcpp/grpcpp.h>

Server::Server(uint16_t port, std::shared_ptr<World> world)
    : world_(world), port_(port), running_(false) {
}

Server::~Server() {
    stop();
}

bool Server::start() {
    if (running_) {
        std::cerr << "Server is already running" << std::endl;
        return false;
    }
    
    try {
        std::string server_address = "0.0.0.0:" + std::to_string(port_);
        
        grpc::ServerBuilder builder;
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
        builder.RegisterService(this);
        
        grpcServer_ = builder.BuildAndStart();
        if (!grpcServer_) {
            std::cerr << "Failed to start gRPC server" << std::endl;
            return false;
        }
        
        running_ = true;
        std::cout << "Server started on " << server_address << std::endl;
        
        // Start session cleanup thread
        shouldStopCleanup_ = false;
        cleanupThread_ = std::make_unique<std::thread>(&Server::sessionCleanupLoop, this);
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to start server: " << e.what() << std::endl;
        running_ = false;
        return false;
    }
}

void Server::stop() {
    if (!running_) {
        return;
    }
    
    std::cout << "Stopping server..." << std::endl;
    running_ = false;
    
    // Stop session cleanup thread
    shouldStopCleanup_ = true;
    if (cleanupThread_ && cleanupThread_->joinable()) {
        cleanupThread_->join();
    }
    
    if (grpcServer_) {
        grpcServer_->Shutdown();
        grpcServer_.reset();
    }
    
    std::cout << "Server stopped" << std::endl;
}

bool Server::isRunning() const {
    return running_;
}


void Server::setWorld(std::shared_ptr<World> world) {
    world_ = world;
}

std::shared_ptr<World> Server::getWorld() const {
    return world_;
}

uint16_t Server::getPort() const {
    return port_;
}

std::string Server::getServerInfo() const {
    return "Minecraft-like Game Server v1.0 on port " + std::to_string(port_);
}

// gRPC service implementations
grpc::Status Server::GetChunk(grpc::ServerContext* context,
                             const blockserver::ChunkRequest* request,
                             blockserver::ChunkResponse* response) {
    std::cout << "[gRPC] GetChunk request: (" << request->x() << ", " << request->y() << ", " << request->z() << ")";
    if (request->has_player_position()) {
        std::cout << " from player: " << request->player_position().player_id();
    }
    std::cout << std::endl;
    
    if (!world_) {
        std::cerr << "No world instance available" << std::endl;
        response->set_success(false);
        response->set_error_message("No world instance available");
        return grpc::Status::OK;
    }
    
    AbsoluteChunkPosition pos{request->x(), request->y(), request->z()};
    auto chunkOpt = world_->chunkAt(pos);
    
    if (!chunkOpt) {
        std::clog << "Chunk not found at (" << request->x() << ", " << request->y() << ", " << request->z() << ")" << std::endl;
        //still succeed, just no chunk data. ok because it's an optional field now
        response->set_success(true);
        return grpc::Status::OK;
    }
    
    auto chunkData = serializeChunk(**chunkOpt);
    response->set_success(true);
    response->set_chunk_data(chunkData.data(), chunkData.size());
    
    std::cout << "[gRPC] GetChunk response for (" << request->x() << ", " << request->y() << ", " << request->z()
              << ") size: " << chunkData.size() << " bytes" << std::endl;
    
    return grpc::Status::OK;
}

grpc::Status Server::GetUpdatedChunks(grpc::ServerContext* context,
                                     const blockserver::UpdatedChunksRequest* request,
                                     blockserver::UpdatedChunksResponse* response) {
    if (!request->has_player_position()) {
        response->set_success(false);
        response->set_error_message("Player position required");
        return grpc::Status::OK;
    }
    
    const auto& playerPos = request->player_position();
    AbsoluteBlockPosition blockPos{playerPos.x(), playerPos.y(), playerPos.z()};
    int32_t renderDistance = request->render_distance();
    
    std::cout << "[gRPC] GetUpdatedChunks request from player: " << playerPos.player_id() 
              << " at (" << playerPos.x() << ", " << playerPos.y() << ", " << playerPos.z() << ")"
              << " render distance: " << renderDistance << std::endl;
    
    auto updatedChunks = getUpdatedChunksInRange(blockPos, renderDistance);
    
    response->set_success(true);
    for (const auto& chunkPos : updatedChunks) {
        auto* chunk = response->add_updated_chunks();
        chunk->set_x(chunkPos.x);
        chunk->set_y(chunkPos.y);
        chunk->set_z(chunkPos.z);
    }
    
    std::cout << "[gRPC] GetUpdatedChunks response: " << updatedChunks.size() << " updated chunks" << std::endl;
    
    return grpc::Status::OK;
}

grpc::Status Server::PlaceBlock(grpc::ServerContext* context,
                               const blockserver::PlaceBlockRequest* request,
                               blockserver::PlaceBlockResponse* response) {
    if (!world_) {
        std::cerr << "No world instance available" << std::endl;
        response->set_success(false);
        response->set_error_message("No world instance available");
        return grpc::Status::OK;
    }
    
    AbsoluteBlockPosition pos{request->x(), request->y(), request->z()};
    Block newBlock = static_cast<Block>(request->block_type());
    
    // Use the World's setBlockIfLoaded method
    bool success = world_->setBlockIfLoaded(pos, newBlock);
    
    if (success) {
        // Mark the chunk as updated
        AbsoluteChunkPosition chunkPos = toAbsoluteChunk(pos);
        markChunkUpdated(chunkPos);
        
        std::string playerInfo = "";
        if (request->has_player_position()) {
            playerInfo = " by player " + request->player_position().player_id();
        }
        std::cout << "Placed block " << request->block_type() << " at (" << request->x() << ", " << request->y() << ", " << request->z() << ")" << playerInfo << std::endl;
        response->set_success(true);
    } else {
        std::cerr << "Failed to place block - chunk not loaded at (" << request->x() << ", " << request->y() << ", " << request->z() << ")" << std::endl;
        response->set_success(false);
        response->set_error_message("Chunk not loaded");
    }
    
    return grpc::Status::OK;
}

grpc::Status Server::BreakBlock(grpc::ServerContext* context,
                               const blockserver::BreakBlockRequest* request,
                               blockserver::BreakBlockResponse* response) {
    // Create a PlaceBlockRequest with Empty block type
    blockserver::PlaceBlockRequest placeRequest;
    if (request->has_player_position()) {
        *placeRequest.mutable_player_position() = request->player_position();
    }
    placeRequest.set_x(request->x());
    placeRequest.set_y(request->y());
    placeRequest.set_z(request->z());
    placeRequest.set_block_type(static_cast<uint32_t>(Block::Empty));
    
    blockserver::PlaceBlockResponse placeResponse;
    auto status = PlaceBlock(context, &placeRequest, &placeResponse);
    
    response->set_success(placeResponse.success());
    response->set_error_message(placeResponse.error_message());
    
    return status;
}

grpc::Status Server::GetBlockAt(grpc::ServerContext* context,
                               const blockserver::GetBlockRequest* request,
                               blockserver::GetBlockResponse* response) {
    if (!world_) {
        std::cerr << "No world instance available" << std::endl;
        response->set_success(false);
        response->set_error_message("No world instance available");
        return grpc::Status::OK;
    }
    
    AbsoluteBlockPosition pos{request->x(), request->y(), request->z()};
    
    // Use the World's getBlockIfLoaded method
    auto blockOpt = world_->getBlockIfLoaded(pos);
    
    response->set_success(true);
    if (blockOpt.has_value()) {
        response->set_block_type(static_cast<uint32_t>(blockOpt.value()));
    } else {
        // If chunk is not loaded, return Empty block
        response->set_block_type(static_cast<uint32_t>(Block::Empty));
    }
    
    return grpc::Status::OK;
}

grpc::Status Server::Ping(grpc::ServerContext* context,
                         const blockserver::PingRequest* request,
                         blockserver::PingResponse* response) {
    response->set_success(true);
    return grpc::Status::OK;
}

grpc::Status Server::GetServerInfo(grpc::ServerContext* context,
                                  const blockserver::ServerInfoRequest* request,
                                  blockserver::ServerInfoResponse* response) {
    response->set_success(true);
    response->set_server_info(getServerInfo());
    return grpc::Status::OK;
}

grpc::Status Server::ConnectPlayer(grpc::ServerContext* context,
                                  const blockserver::ConnectPlayerRequest* request,
                                  blockserver::ConnectPlayerResponse* response) {
    std::cout << "[gRPC] ConnectPlayer request from: " << request->player_name() 
              << " at (" << request->spawn_x() << ", " << request->spawn_y() << ", " << request->spawn_z() << ")" << std::endl;
    
    if (!world_) {
        std::cerr << "No world instance available" << std::endl;
        response->set_success(false);
        response->set_error_message("No world instance available");
        return grpc::Status::OK;
    }
    
    if (request->player_name().empty()) {
        response->set_success(false);
        response->set_error_message("Player name cannot be empty");
        return grpc::Status::OK;
    }
    
    try {
        AbsolutePrecisePosition spawnPos{request->spawn_x(), request->spawn_y(), request->spawn_z()};
        
        // Create player session
        std::string sessionToken = world_->createPlayerSession(request->player_name(), spawnPos);
        
        response->set_success(true);
        response->set_session_token(sessionToken);
        response->set_player_id(request->player_name()); // For now, use name as ID
        response->set_actual_spawn_x(spawnPos.x);
        response->set_actual_spawn_y(spawnPos.y);
        response->set_actual_spawn_z(spawnPos.z);
        
        std::cout << "[gRPC] Player " << request->player_name() << " connected with session: " 
                  << sessionToken.substr(0, 8) << "..." << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to connect player: " << e.what() << std::endl;
        response->set_success(false);
        response->set_error_message("Failed to create player session");
    }
    
    return grpc::Status::OK;
}

grpc::Status Server::RefreshSession(grpc::ServerContext* context,
                                   const blockserver::RefreshSessionRequest* request,
                                   blockserver::RefreshSessionResponse* response) {
    if (!world_) {
        response->set_success(false);
        response->set_error_message("No world instance available");
        return grpc::Status::OK;
    }
    
    if (request->session_token().empty()) {
        response->set_success(false);
        response->set_error_message("Session token cannot be empty");
        return grpc::Status::OK;
    }
    
    bool success = world_->refreshPlayerSession(request->session_token());
    response->set_success(success);
    
    if (!success) {
        response->set_error_message("Invalid or expired session token");
    }
    
    return grpc::Status::OK;
}

grpc::Status Server::UpdatePlayerPosition(grpc::ServerContext* context,
                                         const blockserver::UpdatePlayerPositionRequest* request,
                                         blockserver::UpdatePlayerPositionResponse* response) {
    if (!world_) {
        response->set_success(false);
        response->set_error_message("No world instance available");
        return grpc::Status::OK;
    }
    
    if (request->session_token().empty()) {
        response->set_success(false);
        response->set_error_message("Session token cannot be empty");
        return grpc::Status::OK;
    }
    
    // Validate session first
    if (!world_->isValidSession(request->session_token())) {
        response->set_success(false);
        response->set_error_message("Invalid or expired session token");
        return grpc::Status::OK;
    }
    
    AbsolutePrecisePosition newPos{request->x(), request->y(), request->z()};
    bool success = world_->updatePlayerPosition(request->session_token(), newPos);
    
    response->set_success(success);
    if (!success) {
        response->set_error_message("Failed to update player position");
    }
    
    return grpc::Status::OK;
}

grpc::Status Server::DisconnectPlayer(grpc::ServerContext* context,
                                     const blockserver::DisconnectPlayerRequest* request,
                                     blockserver::DisconnectPlayerResponse* response) {
    if (!world_) {
        response->set_success(false);
        response->set_error_message("No world instance available");
        return grpc::Status::OK;
    }
    
    if (request->session_token().empty()) {
        response->set_success(false);
        response->set_error_message("Session token cannot be empty");
        return grpc::Status::OK;
    }
    
    auto sessionOpt = world_->getPlayerSession(request->session_token());
    if (sessionOpt.has_value()) {
        std::cout << "[gRPC] Disconnecting player: " << sessionOpt.value().playerName << std::endl;
        world_->disconnectPlayerBySession(request->session_token());
        response->set_success(true);
    } else {
        response->set_success(false);
        response->set_error_message("Invalid session token");
    }
    
    return grpc::Status::OK;
}

std::vector<uint8_t> Server::serializeChunk(const ChunkSpan& chunk) {
    return chunk.serialize();
}

void Server::markChunkUpdated(const AbsoluteChunkPosition& pos) {
    std::lock_guard<std::mutex> lock(updatedChunksMutex_);
    updatedChunks_.insert(pos);
}

std::vector<AbsoluteChunkPosition> Server::getUpdatedChunksInRange(const AbsoluteBlockPosition& playerPos, int32_t renderDistance) {
    std::lock_guard<std::mutex> lock(updatedChunksMutex_);
    std::vector<AbsoluteChunkPosition> result;
    
    AbsoluteChunkPosition playerChunk = toAbsoluteChunk(playerPos);
    
    for (const auto& chunkPos : updatedChunks_) {
        // Calculate distance in chunks
        int32_t dx = std::abs(chunkPos.x - playerChunk.x);
        int32_t dy = std::abs(chunkPos.y - playerChunk.y);
        int32_t dz = std::abs(chunkPos.z - playerChunk.z);
        
        // Use Manhattan distance for simplicity
        int32_t distance = std::max({dx, dy, dz});
        
        if (distance <= renderDistance) {
            result.push_back(chunkPos);
        }
    }
    
    // Clear the updated chunks list after returning them
    updatedChunks_.clear();
    
    return result;
}

void Server::sessionCleanupLoop() {
    while (!shouldStopCleanup_) {
        try {
            // Sleep for 1 second between cleanup checks
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            if (world_ && !shouldStopCleanup_) {
                world_->cleanupExpiredSessions();
            }
        } catch (const std::exception& e) {
            std::cerr << "Error in session cleanup: " << e.what() << std::endl;
        }
    }
}