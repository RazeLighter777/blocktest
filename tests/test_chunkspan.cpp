#include <gtest/gtest.h>
#include "chunkspan.h"
#include "position.h"
#include "block.h"
#include "chunkdims.h"

class ChunkSpanTest : public ::testing::Test {
protected:
    void SetUp() override {
        chunkPos = AbsoluteChunkPosition(1, 2, 3);
        chunk = std::make_unique<ChunkSpan>(chunkPos);
    }
    
    void TearDown() override {
        chunk.reset();
    }
    
    AbsoluteChunkPosition chunkPos;
    std::unique_ptr<ChunkSpan> chunk;
};

// Test chunk construction
TEST_F(ChunkSpanTest, Construction) {
    EXPECT_EQ(chunk->position.x, 1);
    EXPECT_EQ(chunk->position.y, 2);
    EXPECT_EQ(chunk->position.z, 3);
    
    // All blocks should be initialized to Empty (0) by default
    ChunkLocalPosition testPos(0, 0, 0);
    EXPECT_EQ(chunk->getBlock(testPos), Block::Empty);
}

// Test setting and getting blocks
TEST_F(ChunkSpanTest, SetAndGetBlock) {
    ChunkLocalPosition pos(5, 10, 8);
    
    // Initially should be empty
    EXPECT_EQ(chunk->getBlock(pos), Block::Empty);
    
    // Set a block
    chunk->setBlock(pos, Block::Stone);
    EXPECT_EQ(chunk->getBlock(pos), Block::Stone);
    
    // Set another block
    chunk->setBlock(pos, Block::Grass);
    EXPECT_EQ(chunk->getBlock(pos), Block::Grass);
    
    // Test different position
    ChunkLocalPosition pos2(1, 1, 1);
    chunk->setBlock(pos2, Block::Wood);
    EXPECT_EQ(chunk->getBlock(pos2), Block::Wood);
    
    // Original position should still be Grass
    EXPECT_EQ(chunk->getBlock(pos), Block::Grass);
}

// Test corner cases and boundaries
TEST_F(ChunkSpanTest, BoundaryAccess) {
    // Test corner positions
    ChunkLocalPosition corner1(0, 0, 0);
    ChunkLocalPosition corner2(CHUNK_WIDTH - 1, CHUNK_HEIGHT - 1, CHUNK_DEPTH - 1);
    
    chunk->setBlock(corner1, Block::Bedrock);
    chunk->setBlock(corner2, Block::Water);
    
    EXPECT_EQ(chunk->getBlock(corner1), Block::Bedrock);
    EXPECT_EQ(chunk->getBlock(corner2), Block::Water);
}

// Test serialization and deserialization
TEST_F(ChunkSpanTest, Serialization) {
    // Set some blocks
    chunk->setBlock(ChunkLocalPosition(0, 0, 0), Block::Stone);
    chunk->setBlock(ChunkLocalPosition(1, 2, 3), Block::Grass);
    chunk->setBlock(ChunkLocalPosition(CHUNK_WIDTH - 1, CHUNK_HEIGHT - 1, CHUNK_DEPTH - 1), Block::Water);
    
    // Serialize
    auto serializedData = chunk->serialize();
    EXPECT_GT(serializedData.size(), 0);
    
    // Deserialize into new chunk
    ChunkSpan deserializedChunk(serializedData);
    
    // Verify blocks are the same
    EXPECT_EQ(deserializedChunk.getBlock(ChunkLocalPosition(0, 0, 0)), Block::Stone);
    EXPECT_EQ(deserializedChunk.getBlock(ChunkLocalPosition(1, 2, 3)), Block::Grass);
    EXPECT_EQ(deserializedChunk.getBlock(ChunkLocalPosition(CHUNK_WIDTH - 1, CHUNK_HEIGHT - 1, CHUNK_DEPTH - 1)), Block::Water);
    
    // Empty positions should still be empty
    EXPECT_EQ(deserializedChunk.getBlock(ChunkLocalPosition(5, 5, 5)), Block::Empty);
}

// Test with array constructor
TEST_F(ChunkSpanTest, ArrayConstructor) {
    std::array<Block, CHUNK_WIDTH * CHUNK_HEIGHT * CHUNK_DEPTH> storage;
    storage.fill(Block::Sand);
    
    // Create chunk with pre-filled storage
    ChunkSpan filledChunk(storage, AbsoluteChunkPosition(10, 20, 30));
    
    // All positions should be Sand
    EXPECT_EQ(filledChunk.getBlock(ChunkLocalPosition(0, 0, 0)), Block::Sand);
    EXPECT_EQ(filledChunk.getBlock(ChunkLocalPosition(5, 5, 5)), Block::Sand);
    EXPECT_EQ(filledChunk.getBlock(ChunkLocalPosition(CHUNK_WIDTH - 1, CHUNK_HEIGHT - 1, CHUNK_DEPTH - 1)), Block::Sand);
    
    // Position should be correct
    EXPECT_EQ(filledChunk.position.x, 10);
    EXPECT_EQ(filledChunk.position.y, 20);
    EXPECT_EQ(filledChunk.position.z, 30);
}

// Test stride calculations (internal consistency)
TEST_F(ChunkSpanTest, StrideValues) {
    EXPECT_EQ(chunk->strideY, CHUNK_WIDTH);
    EXPECT_EQ(chunk->strideZ, CHUNK_WIDTH * CHUNK_HEIGHT);
}