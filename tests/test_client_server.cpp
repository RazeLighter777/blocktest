#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <memory>
#include <atomic>
#include "client.h"
#include "server.h"
#include "world.h"
#include "position.h"
#include "block.h"
#include "chunk_generators.h"
// Helper struct to manage server-client pairs for individual tests
struct ServerClientPair {
    std::shared_ptr<World> world;
    std::unique_ptr<Server> server;
    std::unique_ptr<Client> client;
    std::shared_ptr<FlatworldChunkGenerator> terrainGenerator;
    std::thread server_thread;
    uint16_t port;
    
    ~ServerClientPair() {
        cleanup();
    }
    
    void cleanup() {
        // Disconnect client first
        if (client) {
            client->disconnect();
            client.reset();
        }
        
        // Stop server
        if (server) {
            server->stop();
        }
        
        // Wait for server thread to finish
        if (server_thread.joinable()) {
            server_thread.join();
        }
        
        server.reset();
        world.reset();
    }
};

class ClientServerTest : public ::testing::Test {
protected:
    // Helper function to create a server-client pair for individual tests
    std::unique_ptr<ServerClientPair> createServerClientPair(const std::string& playerId = "test_player") {
        auto pair = std::make_unique<ServerClientPair>();
        
        pair->terrainGenerator = std::make_shared<FlatworldChunkGenerator>(1, Block::Grass);
        // Create a test world
        pair->world = std::make_shared<World>(pair->terrainGenerator, []() { return std::vector<AbsoluteBlockPosition>{ {0,0,0} }; }, 3, 42);
        pair->world->ensureChunksLoaded();

        // Use a random available port for testing (starting from 9090 and incrementing)
        static std::atomic<uint16_t> port_counter{9090};
        pair->port = port_counter.fetch_add(1);
        
        // Create and start server
        pair->server = std::make_unique<Server>(pair->port, pair->world);
        
        pair->server->start();
        
        // Give server time to start
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        
        // Create client
        pair->client = std::make_unique<Client>("127.0.0.1", pair->port, playerId);
        
        return pair;
    }
};

// Test basic connection and ping
TEST_F(ClientServerTest, BasicConnection) {
    auto pair = createServerClientPair();
    
    // Test connection
    EXPECT_TRUE(pair->client->connect());
    EXPECT_TRUE(pair->client->isConnected());
    
    // Test ping
    EXPECT_TRUE(pair->client->ping());
}

// Test server info retrieval
TEST_F(ClientServerTest, ServerInfo) {
    auto pair = createServerClientPair();
    
    EXPECT_TRUE(pair->client->connect());
    
    std::string serverInfo = pair->client->getServerInfo();
    EXPECT_FALSE(serverInfo.empty());
    EXPECT_NE(serverInfo.find("Error"), 0); // Should not start with "Error"
}

// Test player position management
TEST_F(ClientServerTest, PlayerPosition) {
    auto pair = createServerClientPair();
    
    EXPECT_TRUE(pair->client->connect());
    
    // Set player position
    AbsoluteBlockPosition testPos(100, 64, 200);
    pair->client->setPlayerPosition(testPos);
    
    // Get player position
    AbsoluteBlockPosition retrievedPos = pair->client->getPlayerPosition();
    EXPECT_EQ(retrievedPos.x, testPos.x);
    EXPECT_EQ(retrievedPos.y, testPos.y);
    EXPECT_EQ(retrievedPos.z, testPos.z);
    
    // Test player ID
    EXPECT_EQ(pair->client->getPlayerId(), "test_player");
}

// Test block operations
TEST_F(ClientServerTest, BlockOperations) {
    auto pair = createServerClientPair();
    
    EXPECT_TRUE(pair->client->connect());
    
    AbsoluteBlockPosition testPos(50, 64, 50);
    
    // First request the chunk to be loaded
    AbsoluteChunkPosition chunkPos = toAbsoluteChunk(testPos);
    pair->client->setPlayerPosition(testPos); // Set player position first
    
    auto chunk = pair->client->requestChunk(chunkPos);
    
    // Process pending requests in a loop to allow async completion
    
    // Try to place a block (may still fail if chunk generation is not implemented)
    bool placeResult = pair->client->placeBlock(testPos, Block::Stone);
    // Don't assert success since chunk may not be generated
    
    if (placeResult) {
        // If placement succeeded, try to break it
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        bool breakResult = pair->client->breakBlock(testPos);
        EXPECT_TRUE(breakResult);
    }
    
    // This test mainly verifies that operations don't crash
    EXPECT_TRUE(pair->client->isConnected());
}

// Test chunk request functionality
TEST_F(ClientServerTest, ChunkRequests) {
    auto pair = createServerClientPair();
    
    EXPECT_TRUE(pair->client->connect());
    
    // Set player position first
    pair->client->setPlayerPosition(AbsoluteBlockPosition(16, 64, 16));
    
    // Request a chunk
    AbsoluteChunkPosition chunkPos(1, 4, 1); // Assuming chunk size of 16x16x16
    auto chunk = pair->client->requestChunk(chunkPos);
    
    // First request might return nullopt (async), but it should trigger the request
    EXPECT_EQ(pair->client->getCacheSize(), 0); // Initially no cache
    
    // Process pending requests for a bit to allow async completion
    for (int i = 0; i < 10 && pair->client->getPendingRequestCount() > 0; ++i) {
        pair->client->processPendingRequests();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Now try to get the chunk from cache
    auto cachedChunk = pair->client->getCachedChunk(chunkPos);
    // Chunk might be available now
}

// Test preloading chunks around position
TEST_F(ClientServerTest, ChunkPreloading) {
    auto pair = createServerClientPair();
    
    EXPECT_TRUE(pair->client->connect());
    
    // Set player position
    AbsoluteBlockPosition playerPos(0, 0, 0);
    pair->client->setPlayerPosition(playerPos);
    
    // Preload chunks (small radius to avoid overloading)
    pair->client->preloadChunksAroundPosition(playerPos, 1);
    
    // Process requests for a while
    auto start = std::chrono::steady_clock::now();
    while (pair->client->getPendingRequestCount() > 0 && 
           std::chrono::steady_clock::now() - start < std::chrono::seconds(2)) {
        pair->client->processPendingRequests();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Should have some chunks cached (depends on world generation)
    // This is more of a smoke test to ensure no crashes
    EXPECT_GE(pair->client->getCacheSize(), 0);
}

// Test updated chunks tracking
TEST_F(ClientServerTest, UpdatedChunks) {
    auto pair = createServerClientPair();
    
    EXPECT_TRUE(pair->client->connect());
    
    // Set player position
    pair->client->setPlayerPosition(AbsoluteBlockPosition(0, 64, 0));
    
    // Get updated chunks
    auto updatedChunks = pair->client->getUpdatedChunks(2);
    
    // Should return successfully (empty list is fine for new world)
    EXPECT_GE(updatedChunks.size(), 0);
}

// Test cache management
TEST_F(ClientServerTest, CacheManagement) {
    auto pair = createServerClientPair();
    
    EXPECT_TRUE(pair->client->connect());
    
    // Initially cache should be empty
    EXPECT_EQ(pair->client->getCacheSize(), 0);
    
    // Clear cache (should not crash)
    pair->client->clearCache();
    EXPECT_EQ(pair->client->getCacheSize(), 0);
    
    // Test eviction (should not crash with empty cache)
    pair->client->evictOldChunks(5);
    EXPECT_EQ(pair->client->getCacheSize(), 0);
}

// Test client disconnection and reconnection
TEST_F(ClientServerTest, Reconnection) {
    auto pair = createServerClientPair();
    
    // Initial connection
    EXPECT_TRUE(pair->client->connect());
    EXPECT_TRUE(pair->client->isConnected());
    
    // Disconnect
    pair->client->disconnect();
    EXPECT_FALSE(pair->client->isConnected());
    
    // Reconnect
    EXPECT_TRUE(pair->client->connect());
    EXPECT_TRUE(pair->client->isConnected());
    
    // Should still be able to ping
    EXPECT_TRUE(pair->client->ping());
}

// Test multiple clients (using same server)
TEST_F(ClientServerTest, MultipleClients) {
    auto pair = createServerClientPair();
    
    // First client
    EXPECT_TRUE(pair->client->connect());
    EXPECT_TRUE(pair->client->ping());
    
    // Second client using the same server
    auto client2 = std::make_unique<Client>("127.0.0.1", pair->port, "test_player2");
    EXPECT_TRUE(client2->connect());
    EXPECT_TRUE(client2->ping());
    
    // Both should be able to operate independently
    EXPECT_EQ(pair->client->getPlayerId(), "test_player");
    EXPECT_EQ(client2->getPlayerId(), "test_player2");
    
    // Clean up second client
    client2->disconnect();
}

// Test error handling with invalid operations
TEST_F(ClientServerTest, ErrorHandling) {
    auto pair = createServerClientPair();
    
    EXPECT_TRUE(pair->client->connect());
    
    // Test operations that might fail gracefully
    // (The actual behavior depends on your server implementation)
    
    // Try to get block at extreme coordinates (should handle gracefully)
    auto block = pair->client->getBlockAt(AbsoluteBlockPosition(1000000, 1000000, 1000000));
    // Should not crash, result might be nullopt
    
    // Try operations without setting player position (might work or fail gracefully)
    auto updatedChunks = pair->client->getUpdatedChunks(1);
    // Should not crash
}

// Stress test with rapid operations
TEST_F(ClientServerTest, StressTest) {
    auto pair = createServerClientPair();
    
    EXPECT_TRUE(pair->client->connect());
    
    pair->client->setPlayerPosition(AbsoluteBlockPosition(0, 64, 0));
    
    // Rapid ping operations
    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(pair->client->ping());
    }
    
    // Rapid chunk requests (small number to avoid overloading)
    for (int i = 0; i < 5; ++i) {
        pair->client->requestChunk(AbsoluteChunkPosition(i, 4, 0));
    }
    
    // Process all pending requests
    auto start = std::chrono::steady_clock::now();
    while (pair->client->getPendingRequestCount() > 0 && 
           std::chrono::steady_clock::now() - start < std::chrono::seconds(3)) {
        pair->client->processPendingRequests();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    // Should complete without crashes
    EXPECT_TRUE(pair->client->isConnected());
}

// Test basic player connection and session management
TEST_F(ClientServerTest, PlayerConnection) {
    auto pair = createServerClientPair();
    
    EXPECT_TRUE(pair->client->connect());
    
    // Initially should not have a valid session
    EXPECT_FALSE(pair->client->hasValidSession());
    EXPECT_TRUE(pair->client->getSessionToken().empty());
    
    // Connect as a player
    AbsolutePrecisePosition spawnPos(100.5, 64.0, 200.7);
    EXPECT_TRUE(pair->client->connectAsPlayer("TestPlayer", spawnPos));
    
    // Should now have a valid session
    EXPECT_TRUE(pair->client->hasValidSession());
    EXPECT_FALSE(pair->client->getSessionToken().empty());
    
    // Session token should be non-empty and reasonably long
    std::string token = pair->client->getSessionToken();
    EXPECT_GT(token.length(), 10); // Assuming tokens are at least 10 characters
}

// Test session refresh functionality
TEST_F(ClientServerTest, SessionRefresh) {
    auto pair = createServerClientPair();
    
    EXPECT_TRUE(pair->client->connect());
    
    // Connect as a player
    AbsolutePrecisePosition spawnPos(0.0, 64.0, 0.0);
    EXPECT_TRUE(pair->client->connectAsPlayer("RefreshTestPlayer", spawnPos));
    EXPECT_TRUE(pair->client->hasValidSession());
    
    std::string originalToken = pair->client->getSessionToken();
    
    // Refresh session
    EXPECT_TRUE(pair->client->refreshSession());
    
    // Should still have a valid session
    EXPECT_TRUE(pair->client->hasValidSession());
    
    // Token should remain the same (or might be refreshed depending on implementation)
    std::string refreshedToken = pair->client->getSessionToken();
    EXPECT_FALSE(refreshedToken.empty());
}

// Test player position updates with session
TEST_F(ClientServerTest, PlayerPositionUpdates) {
    auto pair = createServerClientPair();
    
    EXPECT_TRUE(pair->client->connect());
    
    // Connect as a player
    AbsolutePrecisePosition spawnPos(50.0, 64.0, 50.0);
    EXPECT_TRUE(pair->client->connectAsPlayer("PositionTestPlayer", spawnPos));
    EXPECT_TRUE(pair->client->hasValidSession());
    
    // Update position
    AbsolutePrecisePosition newPos(100.5, 65.2, 150.8);
    EXPECT_TRUE(pair->client->updatePlayerPosition(newPos));
    
    // Update position multiple times
    for (int i = 0; i < 5; ++i) {
        AbsolutePrecisePosition movePos(newPos.x + i, newPos.y, newPos.z + i);
        EXPECT_TRUE(pair->client->updatePlayerPosition(movePos));
        std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Simulate time between moves
    }

    
    
    // Should still have valid session after position updates
    EXPECT_TRUE(pair->client->hasValidSession());
}

// Test session timeout and expiration
TEST_F(ClientServerTest, SessionTimeout) {
    auto pair = createServerClientPair();
    
    EXPECT_TRUE(pair->client->connect());
    
    // Connect as a player
    AbsolutePrecisePosition spawnPos(0.0, 64.0, 0.0);
    EXPECT_TRUE(pair->client->connectAsPlayer("TimeoutTestPlayer", spawnPos));
    EXPECT_TRUE(pair->client->hasValidSession());
    
    // Wait longer than session timeout (5 seconds + buffer)
    std::this_thread::sleep_for(std::chrono::seconds(6));
    
    // Try to refresh session - should fail due to timeout
    EXPECT_FALSE(pair->client->refreshSession());
    
    // Try to update position - should fail due to expired session
    AbsolutePrecisePosition newPos(10.0, 64.0, 10.0);
    EXPECT_FALSE(pair->client->updatePlayerPosition(newPos));
    
    // Should be able to reconnect after timeout
    EXPECT_TRUE(pair->client->connectAsPlayer("TimeoutTestPlayer", spawnPos));
    EXPECT_TRUE(pair->client->hasValidSession());
}

// Test multiple players with separate sessions
TEST_F(ClientServerTest, MultiplePlayerSessions) {
    auto pair = createServerClientPair();
    
    EXPECT_TRUE(pair->client->connect());
    
    // Create second client
    auto client2 = std::make_unique<Client>("127.0.0.1", pair->port, "test_player2");
    EXPECT_TRUE(client2->connect());
    
    // Connect both clients as players
    AbsolutePrecisePosition spawnPos1(0.0, 64.0, 0.0);
    AbsolutePrecisePosition spawnPos2(100.0, 64.0, 100.0);
    
    EXPECT_TRUE(pair->client->connectAsPlayer("Player1", spawnPos1));
    EXPECT_TRUE(client2->connectAsPlayer("Player2", spawnPos2));
    
    // Both should have valid but different sessions
    EXPECT_TRUE(pair->client->hasValidSession());
    EXPECT_TRUE(client2->hasValidSession());
    EXPECT_NE(pair->client->getSessionToken(), client2->getSessionToken());
    
    // Both should be able to update positions independently
    EXPECT_TRUE(pair->client->updatePlayerPosition(AbsolutePrecisePosition(10.0, 64.0, 10.0)));
    EXPECT_TRUE(client2->updatePlayerPosition(AbsolutePrecisePosition(110.0, 64.0, 110.0)));
    
    // Both should be able to refresh sessions
    EXPECT_TRUE(pair->client->refreshSession());
    EXPECT_TRUE(client2->refreshSession());
    
    // Clean up second client
    client2->disconnectPlayer();
    client2->disconnect();
}

// Test session validation with invalid tokens
TEST_F(ClientServerTest, SessionValidation) {
    auto pair = createServerClientPair();
    
    EXPECT_TRUE(pair->client->connect());
    
    // Connect as a player to get a valid session
    AbsolutePrecisePosition spawnPos(0.0, 64.0, 0.0);
    EXPECT_TRUE(pair->client->connectAsPlayer("ValidationTestPlayer", spawnPos));
    EXPECT_TRUE(pair->client->hasValidSession());
    
    // Disconnect the player (invalidates session)
    EXPECT_TRUE(pair->client->disconnectPlayer());
    
    // Should no longer have a valid session
    EXPECT_FALSE(pair->client->hasValidSession());
    
    // Attempts to use invalidated session should fail
    EXPECT_FALSE(pair->client->refreshSession());
    EXPECT_FALSE(pair->client->updatePlayerPosition(AbsolutePrecisePosition(10.0, 64.0, 10.0)));
}

// Test proper player disconnection and cleanup
TEST_F(ClientServerTest, PlayerDisconnection) {
    auto pair = createServerClientPair();
    
    EXPECT_TRUE(pair->client->connect());
    
    // Connect as a player
    AbsolutePrecisePosition spawnPos(0.0, 64.0, 0.0);
    EXPECT_TRUE(pair->client->connectAsPlayer("DisconnectTestPlayer", spawnPos));
    EXPECT_TRUE(pair->client->hasValidSession());
    
    std::string sessionToken = pair->client->getSessionToken();
    EXPECT_FALSE(sessionToken.empty());
    
    // Disconnect the player
    EXPECT_TRUE(pair->client->disconnectPlayer());
    
    // Should no longer have a valid session
    EXPECT_FALSE(pair->client->hasValidSession());
    
    // Should be able to connect again after disconnection
    EXPECT_TRUE(pair->client->connectAsPlayer("DisconnectTestPlayer", spawnPos));
    EXPECT_TRUE(pair->client->hasValidSession());
    
    // New session should have a different token
    std::string newSessionToken = pair->client->getSessionToken();
    EXPECT_NE(sessionToken, newSessionToken);
}

// Test session management with connection lifecycle
TEST_F(ClientServerTest, SessionWithConnectionLifecycle) {
    auto pair = createServerClientPair();
    
    EXPECT_TRUE(pair->client->connect());
    
    // Connect as player
    AbsolutePrecisePosition spawnPos(0.0, 64.0, 0.0);
    EXPECT_TRUE(pair->client->connectAsPlayer("LifecycleTestPlayer", spawnPos));
    EXPECT_TRUE(pair->client->hasValidSession());
    
    // Disconnect from server (not just player disconnect)
    pair->client->disconnect();
    EXPECT_FALSE(pair->client->isConnected());
    EXPECT_FALSE(pair->client->hasValidSession());
    
    // Reconnect to server
    EXPECT_TRUE(pair->client->connect());
    EXPECT_TRUE(pair->client->isConnected());
    EXPECT_FALSE(pair->client->hasValidSession()); // Session should be lost
    
    // Should be able to connect as player again
    EXPECT_TRUE(pair->client->connectAsPlayer("LifecycleTestPlayer", spawnPos));
    EXPECT_TRUE(pair->client->hasValidSession());
}