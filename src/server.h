#pragma once

#include <memory>
#include <string>
#include <vector>
#include <thread>
#include "rpc/server.h"
#include "world.h"
#include "position.h"
#include "block.h"

class Server {
public:
    // Constructor
    Server(uint16_t port = 8080, 
           std::shared_ptr<World> world = nullptr);
    
    // Destructor
    ~Server();
    
    // Server lifecycle
    bool start();
    void stop();
    bool isRunning() const;
    
    // Run server in blocking mode
    void run();
    
    // Run server in non-blocking mode (separate thread)
    void runAsync();
    
    // World management
    void setWorld(std::shared_ptr<World> world);
    std::shared_ptr<World> getWorld() const;
    
    // Server information
    uint16_t getPort() const;
    std::string getServerInfo() const;

private:
    // RPC method implementations
    std::vector<uint8_t> getChunk(int32_t x, int32_t y, int32_t z);
    bool placeBlock(int64_t x, int64_t y, int64_t z, uint8_t blockType);
    bool breakBlock(int64_t x, int64_t y, int64_t z);
    uint8_t getBlockAt(int64_t x, int64_t y, int64_t z);
    bool ping();
    std::string getServerInfoRpc();
    
    // Helper methods
    void bindRpcMethods();
    std::vector<uint8_t> serializeChunk(const ChunkSpan& chunk);
    
    // Server state
    std::unique_ptr<rpc::server> rpcServer_;
    std::shared_ptr<World> world_;
    uint16_t port_;
    bool running_;
    std::unique_ptr<std::thread> serverThread_;
};