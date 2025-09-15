#pragma once

#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <grpcpp/grpcpp.h>
#include "blockserver.grpc.pb.h"
#include "world.h"
#include "position.h"
#include "block.h"

class Server final : public blockserver::BlockServer::Service {
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
    
    // World management
    void setWorld(std::shared_ptr<World> world);
    std::shared_ptr<World> getWorld() const;
    
    // Server information
    uint16_t getPort() const;
    std::string getServerInfo() const;

    // gRPC service implementations
    grpc::Status GetChunk(grpc::ServerContext* context,
                         const blockserver::ChunkRequest* request,
                         blockserver::ChunkResponse* response) override;
                         
    grpc::Status PlaceBlock(grpc::ServerContext* context,
                           const blockserver::PlaceBlockRequest* request,
                           blockserver::PlaceBlockResponse* response) override;
                           
    grpc::Status BreakBlock(grpc::ServerContext* context,
                           const blockserver::BreakBlockRequest* request,
                           blockserver::BreakBlockResponse* response) override;
                           
    grpc::Status GetBlockAt(grpc::ServerContext* context,
                           const blockserver::GetBlockRequest* request,
                           blockserver::GetBlockResponse* response) override;
                           
    grpc::Status Ping(grpc::ServerContext* context,
                     const blockserver::PingRequest* request,
                     blockserver::PingResponse* response) override;
                     
    grpc::Status GetServerInfo(grpc::ServerContext* context,
                              const blockserver::ServerInfoRequest* request,
                              blockserver::ServerInfoResponse* response) override;

private:
    // Helper methods
    std::vector<uint8_t> serializeChunk(const ChunkSpan& chunk);
    
    // Server state
    std::unique_ptr<grpc::Server> grpcServer_;
    std::shared_ptr<World> world_;
    uint16_t port_;
    bool running_;
    std::unique_ptr<std::thread> serverThread_;
};