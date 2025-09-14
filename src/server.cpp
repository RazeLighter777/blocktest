#include "server.h"
#include "statefulchunkoverlay.h"
#include "chunkdims.h"
#include <iostream>
#include <stdexcept>

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
        rpcServer_ = std::make_unique<rpc::server>(port_);
        bindRpcMethods();
        running_ = true;
        
        std::cout << "Server started on port " << port_ << std::endl;
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
    
    running_ = false;
    
    // Stop the server thread if running async
    if (serverThread_ && serverThread_->joinable()) {
        // rpclib doesn't have a clean shutdown mechanism, so we'll just detach
        serverThread_->detach();
        serverThread_.reset();
    }
    
    rpcServer_.reset();
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
    
    try {
        rpcServer_->run();
    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
        running_ = false;
    }
}

void Server::runAsync() {
    if (!running_ && !start()) {
        throw std::runtime_error("Failed to start server");
    }
    
    serverThread_ = std::make_unique<std::thread>([this]() {
        try {
            rpcServer_->async_run();
        } catch (const std::exception& e) {
            std::cerr << "Server async error: " << e.what() << std::endl;
            running_ = false;
        }
    });
    
    std::cout << "Server running asynchronously on port " << port_ << std::endl;
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

std::vector<uint8_t> Server::getChunk(int32_t x, int32_t y, int32_t z) {
    if (!world_) {
        std::cerr << "No world instance available" << std::endl;
        return {};
    }
    
    AbsoluteChunkPosition pos{x, y, z};
    auto chunkOpt = world_->chunkAt(pos);
    
    if (!chunkOpt) {
        std::cerr << "Chunk not found at position (" << x << ", " << y << ", " << z << ")" << std::endl;
        return {};
    }
    
    return serializeChunk(**chunkOpt);
}

bool Server::placeBlock(int64_t x, int64_t y, int64_t z, uint8_t blockType) {
    if (!world_) {
        std::cerr << "No world instance available" << std::endl;
        return false;
    }
    
    AbsoluteBlockPosition pos{x, y, z};
    AbsoluteChunkPosition chunkPos = toAbsoluteChunk(pos);
    
    // Get or load the chunk
    auto chunkOpt = world_->chunkAt(chunkPos);
    if (!chunkOpt) {
        std::cerr << "Failed to get chunk for block placement at (" << x << ", " << y << ", " << z << ")" << std::endl;
        return false;
    }
    
    // Calculate local position within chunk
    auto localPos = toChunkLocal(pos, chunkPos);
    
    // Validate local position bounds
    if (localPos.x >= CHUNK_WIDTH || localPos.y >= CHUNK_HEIGHT || localPos.z >= CHUNK_DEPTH ||
        localPos.x < 0 || localPos.y < 0 || localPos.z < 0) {
        std::cerr << "Invalid local position (" << localPos.x << ", " << localPos.y << ", " << localPos.z << ")" << std::endl;
        return false;
    }
    
    // Set the block
    auto& chunk = **chunkOpt;
    const std::size_t idx = static_cast<std::size_t>(localPos.z) * chunk.strideZ + 
                          static_cast<std::size_t>(localPos.y) * chunk.strideY + localPos.x;
    
    Block newBlock = static_cast<Block>(blockType);
    chunk.data[idx] = newBlock;
    
    // If the chunk supports persistence, save it
    world_->saveChunk(chunkPos, chunk);
    
    std::cout << "Placed block " << static_cast<int>(blockType) << " at (" << x << ", " << y << ", " << z << ")" << std::endl;
    return true;
}

bool Server::breakBlock(int64_t x, int64_t y, int64_t z) {
    return placeBlock(x, y, z, static_cast<uint8_t>(Block::Empty));
}

uint8_t Server::getBlockAt(int64_t x, int64_t y, int64_t z) {
    if (!world_) {
        std::cerr << "No world instance available" << std::endl;
        return static_cast<uint8_t>(Block::Empty);
    }
    
    AbsoluteBlockPosition pos{x, y, z};
    AbsoluteChunkPosition chunkPos = toAbsoluteChunk(pos);
    
    auto chunkOpt = world_->chunkAt(chunkPos);
    if (!chunkOpt) {
        return static_cast<uint8_t>(Block::Empty);
    }
    
    auto localPos = toChunkLocal(pos, chunkPos);
    
    // Validate bounds
    if (localPos.x >= CHUNK_WIDTH || localPos.y >= CHUNK_HEIGHT || localPos.z >= CHUNK_DEPTH ||
        localPos.x < 0 || localPos.y < 0 || localPos.z < 0) {
        return static_cast<uint8_t>(Block::Empty);
    }
    
    auto& chunk = **chunkOpt;
    const std::size_t idx = static_cast<std::size_t>(localPos.z) * chunk.strideZ + 
                          static_cast<std::size_t>(localPos.y) * chunk.strideY + localPos.x;
    
    return static_cast<uint8_t>(chunk.data[idx]);
}

bool Server::ping() {
    return true;
}

std::string Server::getServerInfoRpc() {
    return getServerInfo();
}

void Server::bindRpcMethods() {
    if (!rpcServer_) {
        throw std::runtime_error("RPC server not initialized");
    }
    
    // Bind chunk operations
    rpcServer_->bind("get_chunk", [this](int32_t x, int32_t y, int32_t z) {
        return getChunk(x, y, z);
    });
    
    // Bind block operations
    rpcServer_->bind("place_block", [this](int64_t x, int64_t y, int64_t z, uint8_t blockType) {
        return placeBlock(x, y, z, blockType);
    });
    
    rpcServer_->bind("break_block", [this](int64_t x, int64_t y, int64_t z) {
        return breakBlock(x, y, z);
    });
    
    rpcServer_->bind("get_block_at", [this](int64_t x, int64_t y, int64_t z) {
        return getBlockAt(x, y, z);
    });
    
    // Bind utility operations
    rpcServer_->bind("ping", [this]() {
        return ping();
    });
    
    rpcServer_->bind("get_server_info", [this]() {
        return getServerInfoRpc();
    });
    
    std::cout << "RPC methods bound successfully" << std::endl;
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