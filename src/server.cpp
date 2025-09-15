#include "server.h"
#include "statefulchunkoverlay.h"
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
    
    if (grpcServer_) {
        grpcServer_->Shutdown();
        grpcServer_.reset();
    }
    
    std::cout << "Server stopped" << std::endl;
}

bool Server::isRunning() const {
    return running_;
}

void Server::run() {
    if (!running_ && !start()) {
        throw std::runtime_error("Failed to start server");
    }
    
    std::cout << "Server running on port " << port_ << ". Press Ctrl+C to stop." << std::endl;
    
    // Wait for server shutdown
    if (grpcServer_) {
        grpcServer_->Wait();
    }
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
    std::cout << "[gRPC] GetChunk request: (" << request->x() << ", " << request->y() << ", " << request->z() << ")" << std::endl;
    
    if (!world_) {
        std::cerr << "No world instance available" << std::endl;
        response->set_success(false);
        response->set_error_message("No world instance available");
        return grpc::Status::OK;
    }
    
    AbsoluteChunkPosition pos{request->x(), request->y(), request->z()};
    auto chunkOpt = world_->chunkAt(pos);
    
    if (!chunkOpt) {
        std::cerr << "Chunk not found at position (" << request->x() << ", " << request->y() << ", " << request->z() << ")" << std::endl;
        response->set_success(false);
        response->set_error_message("Chunk not found");
        return grpc::Status::OK;
    }
    
    auto chunkData = serializeChunk(**chunkOpt);
    response->set_success(true);
    response->set_chunk_data(chunkData.data(), chunkData.size());
    
    std::cout << "[gRPC] GetChunk response for (" << request->x() << ", " << request->y() << ", " << request->z()
              << ") size: " << chunkData.size() << " bytes" << std::endl;
    
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
    AbsoluteChunkPosition chunkPos = toAbsoluteChunk(pos);
    
    // Get or load the chunk
    auto chunkOpt = world_->chunkAt(chunkPos);
    if (!chunkOpt) {
        std::cerr << "Failed to get chunk for block placement at (" << request->x() << ", " << request->y() << ", " << request->z() << ")" << std::endl;
        response->set_success(false);
        response->set_error_message("Failed to get chunk");
        return grpc::Status::OK;
    }
    
    // Calculate local position within chunk
    auto localPos = toChunkLocal(pos, chunkPos);
    
    // Validate local position bounds
    if (localPos.x >= CHUNK_WIDTH || localPos.y >= CHUNK_HEIGHT || localPos.z >= CHUNK_DEPTH ||
        localPos.x < 0 || localPos.y < 0 || localPos.z < 0) {
        std::cerr << "Invalid local position (" << localPos.x << ", " << localPos.y << ", " << localPos.z << ")" << std::endl;
        response->set_success(false);
        response->set_error_message("Invalid local position");
        return grpc::Status::OK;
    }
    
    // Set the block
    auto& chunk = **chunkOpt;
    const std::size_t idx = static_cast<std::size_t>(localPos.z) * chunk.strideZ + 
                          static_cast<std::size_t>(localPos.y) * chunk.strideY + localPos.x;
    
    Block newBlock = static_cast<Block>(request->block_type());
    chunk.data[idx] = newBlock;
    
    // If the chunk supports persistence, save it
    world_->saveChunk(chunkPos, chunk);
    
    std::cout << "Placed block " << request->block_type() << " at (" << request->x() << ", " << request->y() << ", " << request->z() << ")" << std::endl;
    response->set_success(true);
    return grpc::Status::OK;
}

grpc::Status Server::BreakBlock(grpc::ServerContext* context,
                               const blockserver::BreakBlockRequest* request,
                               blockserver::BreakBlockResponse* response) {
    // Create a PlaceBlockRequest with Empty block type
    blockserver::PlaceBlockRequest placeRequest;
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
    AbsoluteChunkPosition chunkPos = toAbsoluteChunk(pos);
    
    auto chunkOpt = world_->chunkAt(chunkPos);
    if (!chunkOpt) {
        response->set_success(true);
        response->set_block_type(static_cast<uint32_t>(Block::Empty));
        return grpc::Status::OK;
    }
    
    auto localPos = toChunkLocal(pos, chunkPos);
    
    // Validate bounds
    if (localPos.x >= CHUNK_WIDTH || localPos.y >= CHUNK_HEIGHT || localPos.z >= CHUNK_DEPTH ||
        localPos.x < 0 || localPos.y < 0 || localPos.z < 0) {
        response->set_success(true);
        response->set_block_type(static_cast<uint32_t>(Block::Empty));
        return grpc::Status::OK;
    }
    
    auto& chunk = **chunkOpt;
    const std::size_t idx = static_cast<std::size_t>(localPos.z) * chunk.strideZ + 
                          static_cast<std::size_t>(localPos.y) * chunk.strideY + localPos.x;
    
    response->set_success(true);
    response->set_block_type(static_cast<uint32_t>(chunk.data[idx]));
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

std::vector<uint8_t> Server::serializeChunk(const ChunkSpan& chunk) {
    // Create a StatefulChunkOverlay from the chunk data
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

std::vector<uint8_t> Server::serializeChunk(const ChunkSpan& chunk) {
    // Create a StatefulChunkOverlay from the chunk data
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