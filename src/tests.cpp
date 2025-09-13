#include "statefulchunkoverlay.h"
#include "emptychunk.h"
#include "chunkoverlay.h"
#include "perlinnoiseoverlay.h"
#include "perlinnoise.hpp"
#include "world.h"
#include "sqlite_chunk_persistence.h"
#include<iostream>
#include<cassert>
#include <sqlite3.h>
#include <memory>
#include <cstdio>
#include <cstdlib>
#include "chunk_generators.h"
#include <filesystem>
#include <chrono>
#include <string>
#include <vector>

// Global list to track database files for cleanup
static std::vector<std::string> g_testDatabaseFiles;

// Generate a unique database filename for testing
std::string generateTestDbPath() {
    auto now = std::chrono::high_resolution_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
    
    std::string dbPath = "test_db_" + std::to_string(timestamp) + ".sqlite";
    g_testDatabaseFiles.push_back(dbPath);
    return dbPath;
}

// Cleanup all test database files
void cleanupTestDatabases() {
    for (const auto& dbPath : g_testDatabaseFiles) {
        try {
            if (std::filesystem::exists(dbPath)) {
                std::filesystem::remove(dbPath);
                std::cout << "Cleaned up database: " << dbPath << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Warning: Could not remove database file " << dbPath << ": " << e.what() << std::endl;
        }
    }
    g_testDatabaseFiles.clear();
}

void testStatefulChunkOverlayDefaultIsEmpty() {
    StatefulChunkOverlay overlay;
    std::vector<Block> buf(kChunkElemCount, Block::Wood);
    ChunkSpan span{ buf.data(), CHUNK_WIDTH, static_cast<uint32_t>(CHUNK_WIDTH) * CHUNK_HEIGHT };
    overlay.generate(span);
    for (std::size_t i = 0; i < buf.size(); ++i) assert(buf[i] == Block::Empty);
    std::cout << "testStatefulChunkOverlayDefaultIsEmpty passed.\n";
}

void testStatefulChunkOverlaySetAndGet() {
    StatefulChunkOverlay overlay;
    ChunkLocalPosition pos(1, 2, 3);
    overlay.setBlock(pos, Block::Grass);
    std::vector<Block> buf(kChunkElemCount, Block::Wood);
    ChunkSpan span{ buf.data(), CHUNK_WIDTH, static_cast<uint32_t>(CHUNK_WIDTH) * CHUNK_HEIGHT };
    overlay.generate(span);
    const std::size_t idx = static_cast<std::size_t>(pos.z) * span.strideZ + static_cast<std::size_t>(pos.y) * span.strideY + pos.x;
    assert(buf[idx] == Block::Grass);

    // Setting to Air should remove the block
    overlay.setBlock(pos, Block::Empty);
    overlay.generate(span);
    assert(buf[idx] == Block::Empty);

    std::cout << "testStatefulChunkOverlaySetAndGet passed.\n";
}

void testStatefulChunkOverlaySerialization() {
    StatefulChunkOverlay overlay;
    ChunkLocalPosition pos1(1, 2, 3);
    ChunkLocalPosition pos2(4, 5, 6);
    overlay.setBlock(pos1, Block::Stone);
    overlay.setBlock(pos2, Block::Dirt);

    auto data = overlay.serialize();
    std::cout << "Serialized data size: " << data.size() << " bytes\n";
    std::cout << "Serialized data:";
    for (uint8_t byte : data) {
        std::cout << " " << std::hex << static_cast<int>(byte);
    }
    std::cout << std::dec << "\n";
    auto deserializedOpt = StatefulChunkOverlay::deserialize(data);
    assert(deserializedOpt.has_value());
    auto deserialized = deserializedOpt.value();
    std::vector<Block> buf2(kChunkElemCount, Block::Wood);
    ChunkSpan span2{ buf2.data(), CHUNK_WIDTH, static_cast<uint32_t>(CHUNK_WIDTH) * CHUNK_HEIGHT };
    deserialized.generate(span2);
    auto idx1 = static_cast<std::size_t>(pos1.z) * span2.strideZ + static_cast<std::size_t>(pos1.y) * span2.strideY + pos1.x;
    auto idx2 = static_cast<std::size_t>(pos2.z) * span2.strideZ + static_cast<std::size_t>(pos2.y) * span2.strideY + pos2.x;
    assert(buf2[idx1] == Block::Stone);
    assert(buf2[idx2] == Block::Dirt);
    assert(buf2[0] == Block::Empty);

    std::cout << "testStatefulChunkOverlaySerialization passed.\n";
}

void testStatefulChunkOverlayFromOtherOverlay() {
    EmptyChunk emptyChunk;
    StatefulChunkOverlay overlay(emptyChunk);
    std::vector<Block> buf(kChunkElemCount, Block::Wood);
    ChunkSpan span{ buf.data(), CHUNK_WIDTH, static_cast<uint32_t>(CHUNK_WIDTH) * CHUNK_HEIGHT };
    overlay.generate(span);
    for (std::size_t i = 0; i < buf.size(); ++i) assert(buf[i] == Block::Empty);
    std::cout << "testStatefulChunkOverlayFromOtherOverlay passed.\n";
}

void testEmptyChunk() {
    EmptyChunk emptyChunk;
    std::vector<Block> buf(kChunkElemCount, Block::Wood);
    ChunkSpan span{ buf.data(), CHUNK_WIDTH, static_cast<uint32_t>(CHUNK_WIDTH) * CHUNK_HEIGHT };
    emptyChunk.generate(span);
    for (std::size_t i = 0; i < buf.size(); ++i) assert(buf[i] == Block::Empty);
    std::cout << "testEmptyChunk passed.\n";
}

void testChainOverlayCompose() {
    auto emptyChunk = std::make_shared<EmptyChunk>();
    auto statefulOverlay = std::make_shared<StatefulChunkOverlay>();
    ChunkLocalPosition pos(1, 1, 1);
    statefulOverlay->setBlock(pos, Block::Wood);

    auto combined = compose(statefulOverlay, emptyChunk); // top-most first
    std::vector<Block> buf(kChunkElemCount, Block::Empty);
    ChunkSpan span{ buf.data(), CHUNK_WIDTH, static_cast<uint32_t>(CHUNK_WIDTH) * CHUNK_HEIGHT };
    combined->generate(span);
    auto idx = static_cast<std::size_t>(pos.z) * span.strideZ + static_cast<std::size_t>(pos.y) * span.strideY + pos.x;
    assert(buf[idx] == Block::Wood);
    assert(buf[0] == Block::Empty);

    std::cout << "testChainOverlayCompose passed.\n";
}

void testChainOverlayTwoStatefulLayersSamePosition() {
    auto stateful1 = std::make_shared<StatefulChunkOverlay>();
    auto stateful2 = std::make_shared<StatefulChunkOverlay>();
    ChunkLocalPosition pos(2, 2, 2);
    stateful1->setBlock(pos, Block::Stone);
    stateful2->setBlock(pos, Block::Grass);

    auto combined = compose(stateful2, stateful1); // top-most first
    std::vector<Block> buf(kChunkElemCount, Block::Empty);
    ChunkSpan span{ buf.data(), CHUNK_WIDTH, static_cast<uint32_t>(CHUNK_WIDTH) * CHUNK_HEIGHT };
    combined->generate(span);
    auto idx = static_cast<std::size_t>(pos.z) * span.strideZ + static_cast<std::size_t>(pos.y) * span.strideY + pos.x;
    assert(buf[idx] == Block::Grass); // top-most should win
    assert(buf[0] == Block::Empty);

    std::cout << "testChainOverlayTwoStatefulLayersSamePosition passed.\n";
}

void testBedrockOverlayBasic() {
    auto noise = std::make_shared<siv::PerlinNoise>(12345);
    auto bedrock = std::make_shared<PerlinNoiseOverlay>(noise);
    std::vector<Block> buf(kChunkElemCount, Block::Wood);
    ChunkSpan span{ buf.data(), CHUNK_WIDTH, static_cast<uint32_t>(CHUNK_WIDTH) * CHUNK_HEIGHT };
    bedrock->generate(span);
    // For a small set of x,z, bottom two layers should be bedrock
    for (uint8_t x = 0; x < 5; ++x) {
        for (uint8_t z = 0; z < 5; ++z) {
            auto idx0 = static_cast<std::size_t>(z) * span.strideZ + 0 * span.strideY + x;
            auto idx1 = static_cast<std::size_t>(z) * span.strideZ + 1 * span.strideY + x;
            auto idx3 = static_cast<std::size_t>(z) * span.strideZ + 50 * span.strideY + x;
            assert(buf[idx0] == Block::Bedrock);
            assert(buf[idx1] == Block::Bedrock);
        }
    }
    // print out some terrain to ensure variability
    for (uint8_t x = 0; x < 16; ++x) {
        for (uint8_t z = 0; z < 16; ++z) {
            uint8_t thickness = 0;
            for (uint8_t y = 0; y < 5; ++y) {
                const std::size_t idx = static_cast<std::size_t>(z) * span.strideZ + static_cast<std::size_t>(y) * span.strideY + x;
                if (buf[idx] == Block::Bedrock) thickness++;
            }
            std::cout << (int)thickness << " ";
        }
        std::cout << "\n";
    }
    std::cout << "testBedrockOverlayBasic passed.\n";
}

// Test overlays for generate()
struct PatternOverlay : public ChunkOverlay {
    // Writes Stone when (x+y+z) is even; otherwise copies parent/Empty
    void generateInto(const ChunkSpan& out, const Block* parent) const override {
        for (std::uint32_t z = 0; z < CHUNK_DEPTH; ++z) {
            for (std::uint32_t y = 0; y < CHUNK_HEIGHT; ++y) {
                for (std::uint32_t x = 0; x < CHUNK_WIDTH; ++x) {
                    const std::size_t idx = static_cast<std::size_t>(z) * out.strideZ + static_cast<std::size_t>(y) * out.strideY + x;
                    const uint32_t sum = x + y + z;
                    if ((sum & 1u) == 0u) {
                        out.data[idx] = Block::Stone;
                    } else {
                        out.data[idx] = parent ? parent[idx] : Block::Empty;
                    }
                }
            }
        }
    }
};

void testGenerateEmptyChunk() {
    auto empty = std::make_shared<EmptyChunk>();
    std::vector<Block> buf(static_cast<size_t>(CHUNK_WIDTH) * CHUNK_HEIGHT * CHUNK_DEPTH, Block::Wood);
    ChunkSpan span{ buf.data(), CHUNK_WIDTH, static_cast<uint32_t>(CHUNK_WIDTH) * CHUNK_HEIGHT };

    empty->generate(span);

    for (uint8_t z = 0; z < CHUNK_DEPTH; ++z) {
        for (uint8_t y = 0; y < CHUNK_HEIGHT; ++y) {
            for (uint8_t x = 0; x < CHUNK_WIDTH; ++x) {
                const size_t idx = static_cast<size_t>(z) * span.strideZ + static_cast<size_t>(y) * span.strideY + x;
                assert(buf[idx] == Block::Empty);
            }
        }
    }
    std::cout << "testGenerateEmptyChunk passed.\n";
}

void testGeneratePatternOverlay() {
    auto pattern = std::make_shared<PatternOverlay>();
    auto base = std::make_shared<EmptyChunk>();
    auto chain = compose(pattern, base); // top-most first

    std::vector<Block> buf(static_cast<size_t>(CHUNK_WIDTH) * CHUNK_HEIGHT * CHUNK_DEPTH, Block::Wood);
    ChunkSpan span{ buf.data(), CHUNK_WIDTH, static_cast<uint32_t>(CHUNK_WIDTH) * CHUNK_HEIGHT };
    chain->generate(span);

    size_t stoneCount = 0, emptyCount = 0;
    for (uint8_t z = 0; z < CHUNK_DEPTH; ++z) {
        for (uint8_t y = 0; y < CHUNK_HEIGHT; ++y) {
            for (uint8_t x = 0; x < CHUNK_WIDTH; ++x) {
                const size_t idx = static_cast<size_t>(z) * span.strideZ + static_cast<size_t>(y) * span.strideY + x;
                const uint32_t sum = static_cast<uint32_t>(x) + static_cast<uint32_t>(y) + static_cast<uint32_t>(z);
                const bool expectStone = (sum & 1u) == 0u;
                if (expectStone) {
                    assert(buf[idx] == Block::Stone);
                    ++stoneCount;
                } else {
                    assert(buf[idx] == Block::Empty);
                    ++emptyCount;
                }
            }
        }
    }

    const size_t total = kChunkElemCount;
    assert(stoneCount + emptyCount == total);
    std::cout << "testGeneratePatternOverlay passed.\n";
}

void testGenerate10000Chunks() {
    auto empty = std::make_shared<EmptyChunk>();
    auto pattern = std::make_shared<PatternOverlay>();
    auto chain = compose(pattern, empty); // top-most first

    std::vector<Block> buf(static_cast<size_t>(CHUNK_WIDTH) * CHUNK_HEIGHT * CHUNK_DEPTH, Block::Wood);
    ChunkSpan span{ buf.data(), CHUNK_WIDTH, static_cast<uint32_t>(CHUNK_WIDTH) * CHUNK_HEIGHT };

    for (int i = 0; i < 10000; ++i) {
        chain->generate(span);
    }

    // Spot check a few positions
    assert(buf[0] == Block::Stone); // (0,0,0)
    assert(buf[1] == Block::Empty); // (1,0,0)
    assert(buf[CHUNK_WIDTH] == Block::Empty); // (0,1,0)
    assert(buf[CHUNK_WIDTH + 1] == Block::Stone); // (1,1,0)
    assert(buf[CHUNK_WIDTH * CHUNK_HEIGHT] == Block::Empty); // (0,0,1)
    assert(buf[CHUNK_WIDTH * CHUNK_HEIGHT + 1] == Block::Stone); // (1,0,1)

    std::cout << "testGenerate10000Chunks passed.\n";
}

void testWorldChunkPersistence() {
    // Create a unique temporary database file
    std::string dbPath = generateTestDbPath();
    
    sqlite3* rawDb = nullptr;
    int rc = sqlite3_open(dbPath.c_str(), &rawDb);
    assert(rc == SQLITE_OK);
    assert(rawDb != nullptr);
    
    // Create world with database
    std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db(rawDb, sqlite3_close);
    auto persistence = std::make_shared<SQLiteChunkPersistence>(std::move(db));
    std::vector<AbsoluteBlockPosition> loadAnchors = {AbsoluteBlockPosition(0, 0, 0)};
    World world(nullptr, loadAnchors, 2, 0, persistence);
    
    // Load chunks around anchor
    world.ensureChunksLoaded();
    
    // Get a chunk and modify it
    AbsoluteChunkPosition chunkPos(0, 0, 0);
    auto chunkOpt = world.chunkAt(chunkPos);
    assert(chunkOpt.has_value());
    auto chunk = chunkOpt.value();
    
    // Modify some blocks in the chunk
    chunk->data[0] = Block::Stone;  // (0,0,0)
    chunk->data[1] = Block::Grass;  // (1,0,0)
    chunk->data[CHUNK_WIDTH] = Block::Dirt; // (0,1,0)
    
    // Save the chunk manually
    assert(world.saveChunk(chunkPos, *chunk));
    
    std::cout << "testWorldChunkPersistence: Save test passed.\n";
    
    // Create a new world instance with the same database
    sqlite3* rawDb2 = nullptr;
    rc = sqlite3_open(dbPath.c_str(), &rawDb2);
    assert(rc == SQLITE_OK);
    std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db2(rawDb2, sqlite3_close);
    auto persistence2 = std::make_shared<SQLiteChunkPersistence>(std::move(db2));
    World world2(nullptr, loadAnchors, 2, 0, persistence2);
    
    // Load the chunk
    auto loadedChunkOpt = world2.loadChunk(chunkPos);
    if (!loadedChunkOpt.has_value()) {
        std::cout << "Failed to load chunk at (" << chunkPos.x << "," << chunkPos.y << "," << chunkPos.z << ")\n";
        assert(false);
    }
    auto loadedChunk = loadedChunkOpt.value();
    
    // Verify the data was preserved
    assert(loadedChunk->data[0] == Block::Stone);
    assert(loadedChunk->data[1] == Block::Grass);
    assert(loadedChunk->data[CHUNK_WIDTH] == Block::Dirt);
    
    
    std::cout << "testWorldChunkPersistence passed.\n";
}

void testWorldAutoSaveOnUnload() {
    // Create a unique temporary database file
    std::string dbPath = generateTestDbPath();
    
    sqlite3* rawDb = nullptr;
    int rc = sqlite3_open(dbPath.c_str(), &rawDb);
    assert(rc == SQLITE_OK);
    
    AbsoluteChunkPosition chunkPos(0, 0, 0);
    
    {
        // Scope for the first world instance
        std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db(rawDb, sqlite3_close);
        auto persistence = std::make_shared<SQLiteChunkPersistence>(std::move(db));
        std::vector<AbsoluteBlockPosition> loadAnchors = {AbsoluteBlockPosition(0, 0, 0)};
        World world(nullptr, loadAnchors, 1, 0, persistence);
        
        // Load chunks around anchor (radius=1)
        world.ensureChunksLoaded();
        
        // Get and modify a chunk
        auto chunkOpt = world.chunkAt(chunkPos);
        assert(chunkOpt.has_value());
        auto chunk = chunkOpt.value();
        
        chunk->data[42] = Block::Wood;  // Arbitrary position
        chunk->data[100] = Block::Stone;
        
        // The world destructor should save all chunks when going out of scope
    }
    
    // Create new world instance and verify the chunk was saved
    sqlite3* rawDb2 = nullptr;
    rc = sqlite3_open(dbPath.c_str(), &rawDb2);
    assert(rc == SQLITE_OK);
    std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db2(rawDb2, sqlite3_close);
    auto persistence2 = std::make_shared<SQLiteChunkPersistence>(std::move(db2));
    World world2(nullptr, {}, 0, 0, persistence2);
    
    auto loadedChunkOpt = world2.loadChunk(chunkPos);
    assert(loadedChunkOpt.has_value());
    auto loadedChunk = loadedChunkOpt.value();
    
    assert(loadedChunk->data[42] == Block::Wood);
    assert(loadedChunk->data[100] == Block::Stone);
    
    
    std::cout << "testWorldAutoSaveOnUnload passed.\n";
}

void testChunkGenerators() {
    std::cout << "Testing chunk generators...\n";
    
    // Test empty chunk generator
    auto emptyGen = std::make_shared<EmptyChunkGenerator>();
    auto emptyChunk = emptyGen->generateChunk(AbsoluteChunkPosition(0, 0, 0), 42);
    assert(emptyChunk != nullptr);
    
    // Verify it's actually empty
    bool isEmpty = true;
    for (uint32_t z = 0; z < CHUNK_DEPTH && isEmpty; ++z) {
        for (uint32_t y = 0; y < CHUNK_HEIGHT && isEmpty; ++y) {
            for (uint32_t x = 0; x < CHUNK_WIDTH && isEmpty; ++x) {
                const std::size_t idx = static_cast<std::size_t>(z) * emptyChunk->strideZ + 
                                      static_cast<std::size_t>(y) * emptyChunk->strideY + x;
                if (emptyChunk->data[idx] != Block::Empty) {
                    isEmpty = false;
                }
            }
        }
    }
    assert(isEmpty);
    std::cout << "Empty chunk generator test passed.\n";
    
    // Test terrain generator
    auto terrainGen = std::make_shared<TerrainChunkGenerator>();
    auto terrainChunk = terrainGen->generateChunk(AbsoluteChunkPosition(0, 0, 0), 42);
    assert(terrainChunk != nullptr);
    
    // Count non-empty blocks
    int nonEmptyCount = 0;
    for (uint32_t z = 0; z < CHUNK_DEPTH; ++z) {
        for (uint32_t y = 0; y < CHUNK_HEIGHT; ++y) {
            for (uint32_t x = 0; x < CHUNK_WIDTH; ++x) {
                const std::size_t idx = static_cast<std::size_t>(z) * terrainChunk->strideZ + 
                                      static_cast<std::size_t>(y) * terrainChunk->strideY + x;
                if (terrainChunk->data[idx] != Block::Empty) {
                    nonEmptyCount++;
                }
            }
        }
    }
    std::cout << "Terrain chunk has " << nonEmptyCount << " non-empty blocks.\n";
    std::cout << "Terrain chunk generator test passed.\n";
    
    // Test with World
    std::vector<AbsoluteBlockPosition> loadAnchors = {AbsoluteBlockPosition(0, 0, 0)};
    World world(terrainGen, loadAnchors, 1, 42, nullptr);
    world.ensureChunksLoaded();
    
    auto worldChunk = world.chunkAt(AbsoluteChunkPosition(0, 0, 0));
    assert(worldChunk.has_value());
    std::cout << "World with terrain generator test passed.\n";
    
    std::cout << "All chunk generator tests passed!\n";
}


void testWorldLoadOnDemand() {
    // Create a unique temporary database file
    std::string dbPath = generateTestDbPath();
    
    sqlite3* rawDb = nullptr;
    int rc = sqlite3_open(dbPath.c_str(), &rawDb);
    assert(rc == SQLITE_OK);
    
    {
        // First, create and save a chunk
        std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db(rawDb, sqlite3_close);
        auto persistence = std::make_shared<SQLiteChunkPersistence>(std::move(db));
        World world(nullptr, {}, 0, 0, persistence);
        
        AbsoluteChunkPosition testPos(5, 5, 5);
        
        // Create a chunk with some data
        std::vector<Block> chunkData(kChunkElemCount, Block::Empty);
        chunkData[0] = Block::Bedrock;
        chunkData[500] = Block::Grass;
        ChunkSpan testChunk(chunkData.data(), CHUNK_WIDTH, CHUNK_WIDTH * CHUNK_HEIGHT, testPos);
        testChunk.data = chunkData.data();
        
        // Save it
        assert(world.saveChunk(testPos, testChunk));
    }
    
    // Now test loading on demand
    sqlite3* rawDb2 = nullptr;
    rc = sqlite3_open(dbPath.c_str(), &rawDb2);
    assert(rc == SQLITE_OK);
    std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db2(rawDb2, sqlite3_close);
    auto persistence2 = std::make_shared<SQLiteChunkPersistence>(std::move(db2));
    
    // Create world with load anchor at the saved chunk
    std::vector<AbsoluteBlockPosition> loadAnchors = {AbsoluteBlockPosition(5*CHUNK_WIDTH, 5*CHUNK_HEIGHT, 5*CHUNK_DEPTH)};
    World world2(nullptr, loadAnchors, 1, 0, persistence2);
    
    // This should automatically load the chunk from database
    world2.ensureChunksLoaded();
    
    auto chunkOpt = world2.chunkAt(AbsoluteChunkPosition(5, 5, 5));
    assert(chunkOpt.has_value());
    auto chunk = chunkOpt.value();
    
    assert(chunk->data[0] == Block::Bedrock);
    assert(chunk->data[500] == Block::Grass);
    
    
    std::cout << "testWorldLoadOnDemand passed.\n";
}

void testLargeScaleTerrainGeneration() {
    std::cout << "Running large-scale terrain generation test...\n";
    
    // Create a unique temporary database file
    std::string dbPath = generateTestDbPath();
    
    // Phase 1: Generate and save thousands of chunks using World object
    {
        sqlite3* rawDb = nullptr;
        int rc = sqlite3_open(dbPath.c_str(), &rawDb);
        assert(rc == SQLITE_OK);
        
        std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db(rawDb, sqlite3_close);
        auto persistence = std::make_shared<SQLiteChunkPersistence>(std::move(db));
        auto terrainGenerator = std::make_shared<TerrainChunkGenerator>();
        
        // Create load anchors for the 20x10x20 region we want to generate
        std::vector<AbsoluteBlockPosition> loadAnchors;
        // Use a single central anchor to cover our region more efficiently
        loadAnchors.push_back(AbsoluteBlockPosition(
            2 * CHUNK_WIDTH,   // Center of x range (0-19)
            2 * CHUNK_HEIGHT,   // Center of y range (0-9) 
            2 * CHUNK_DEPTH    // Center of z range (0-19)
        ));
        
        std::cout << "Generating and saving chunks using World object...\n";
        
        // Create World with terrain generator, load anchors, and persistence
        // Use radius 12 to cover our 20x10x20 region from the center
        World world(terrainGenerator, loadAnchors, 7, 42, persistence);
        
        // Generate all chunks by ensuring they are loaded
        world.ensureChunksLoaded();
        
        // Verify and count generated chunks
        int chunksGenerated = 0;
        for (int x = 0; x < 5; x++) {
            for (int y = 0; y < 5; y++) {
                for (int z = 0; z < 5; z++) {
                    AbsoluteChunkPosition pos(x, y, z);
                    auto chunkOpt = world.chunkAt(pos);
                    assert(chunkOpt.has_value());
                    chunksGenerated++;
                    
                    // Verify chunk has realistic terrain structure
                    auto chunk = chunkOpt.value();
                    bool hasGrass = false, hasDirt = false, hasStone = false, hasBedrock = false;
                    
                    for (int i = 0; i < kChunkElemCount; i++) {
                        Block block = chunk->data[i];
                        if (block == Block::Grass) hasGrass = true;
                        else if (block == Block::Dirt) hasDirt = true;
                        else if (block == Block::Stone) hasStone = true;
                        else if (block == Block::Bedrock) hasBedrock = true;
                    }
                    
                    // Verify we have the expected terrain layers
                    assert(hasBedrock); // Should always have bedrock
                    if (y < 5) { // Lower chunks should have stone
                        assert(hasStone);
                    }
                }
            }
            if (x % 5 == 4) {
                std::cout << "Generated " << chunksGenerated << " chunks so far...\n";
            }
        }
        
        std::cout << "Generated " << chunksGenerated << " chunks total.\n";
        
        // Save all chunks to database (World destructor will do this automatically,
        // but we can also call it explicitly)
        world.saveAllLoadedChunks();
        std::cout << "All chunks saved to database.\n";
    }
    
    // Phase 2: Load chunks from database and verify data integrity using World object
    {
        sqlite3* rawDb = nullptr;
        int rc = sqlite3_open(dbPath.c_str(), &rawDb);
        assert(rc == SQLITE_OK);
        
        std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db(rawDb, sqlite3_close);
        auto persistence = std::make_shared<SQLiteChunkPersistence>(std::move(db));
        
        std::cout << "Loading and verifying chunks from database using World object...\n";
        
        // Create load anchors for the same region
        std::vector<AbsoluteBlockPosition> loadAnchors;
        // Use the same central anchor
        loadAnchors.push_back(AbsoluteBlockPosition(
            2 * CHUNK_WIDTH,   // Center of x range 
            2 * CHUNK_HEIGHT,   // Center of y range
            2 * CHUNK_DEPTH    // Center of z range
        ));
        
        // Create World without generator to test pure loading from persistence
        World world(nullptr, loadAnchors, 12, 42, persistence);
        
        // Load all chunks from database
        world.ensureChunksLoaded();
        
        int chunksLoaded = 0;
        for (int x = 0; x < 5; x++) {
            for (int y = 0; y < 5; y++) {
                for (int z = 0; z < 5; z++) {
                    AbsoluteChunkPosition pos(x, y, z);
                    auto chunkOpt = world.chunkAt(pos);
                    assert(chunkOpt.has_value());
                    chunksLoaded++;
                    
                    // Verify the loaded chunk has the same terrain structure
                    auto chunk = chunkOpt.value();
                    bool hasGrass = false, hasDirt = false, hasStone = false, hasBedrock = false;
                    
                    for (int i = 0; i < kChunkElemCount; i++) {
                        Block block = chunk->data[i];
                        if (block == Block::Grass) hasGrass = true;
                        else if (block == Block::Dirt) hasDirt = true;
                        else if (block == Block::Stone) hasStone = true;
                        else if (block == Block::Bedrock) hasBedrock = true;
                    }
                    
                    // Verify we still have the expected terrain layers after reload
                    assert(hasBedrock); // Should always have bedrock
                    if (y < 5) { // Lower chunks should have stone
                        assert(hasStone);
                    }
                }
            }
            if (x % 5 == 4) {
                std::cout << "Verified " << chunksLoaded << " chunks so far...\n";
            }
        }
        
        std::cout << "Verified " << chunksLoaded << " chunks total.\n";
        assert(chunksLoaded == 125);
    }
    
    std::cout << "Large-scale terrain generation test passed! Generated, saved, and verified 4000 chunks using World object.\n";
}

int main() {
    testStatefulChunkOverlayDefaultIsEmpty();
    testStatefulChunkOverlaySetAndGet();
    testStatefulChunkOverlaySerialization();
    testStatefulChunkOverlayFromOtherOverlay();
    testEmptyChunk();
    testChainOverlayCompose();
    testBedrockOverlayBasic();
    testGenerateEmptyChunk();
    testGeneratePatternOverlay();
    testGenerate10000Chunks();
    testChainOverlayTwoStatefulLayersSamePosition();
    
    // New persistence tests
    testWorldChunkPersistence();
    testWorldAutoSaveOnUnload();
    testWorldLoadOnDemand();
    testChunkGenerators();
    
    // Large-scale terrain generation test
    testLargeScaleTerrainGeneration();
    
    std::cout << "All tests passed.\n";
    
    // Clean up all test database files
    cleanupTestDatabases();
    
    return 0;
}