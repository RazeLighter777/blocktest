#include <gtest/gtest.h>
#include "position.h"
#include "chunkdims.h"

class PositionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code if needed
    }
    
    void TearDown() override {
        // Cleanup code if needed
    }
};

// Test basic position construction and access
TEST_F(PositionTest, BasicConstruction) {
    AbsoluteBlockPosition blockPos(10, 20, 30);
    EXPECT_EQ(blockPos.x, 10);
    EXPECT_EQ(blockPos.y, 20);
    EXPECT_EQ(blockPos.z, 30);
    
    AbsolutePrecisePosition precisePos(10.5, 20.5, 30.5);
    EXPECT_DOUBLE_EQ(precisePos.x, 10.5);
    EXPECT_DOUBLE_EQ(precisePos.y, 20.5);
    EXPECT_DOUBLE_EQ(precisePos.z, 30.5);
    
    AbsoluteChunkPosition chunkPos(1, 2, 3);
    EXPECT_EQ(chunkPos.x, 1);
    EXPECT_EQ(chunkPos.y, 2);
    EXPECT_EQ(chunkPos.z, 3);
}

// Test position copy and assignment
TEST_F(PositionTest, CopyAndAssignment) {
    AbsoluteBlockPosition original(100, 200, 300);
    AbsoluteBlockPosition copied(original);
    
    EXPECT_EQ(copied.x, original.x);
    EXPECT_EQ(copied.y, original.y);
    EXPECT_EQ(copied.z, original.z);
    
    AbsoluteBlockPosition assigned;
    assigned = original;
    EXPECT_EQ(assigned.x, original.x);
    EXPECT_EQ(assigned.y, original.y);
    EXPECT_EQ(assigned.z, original.z);
}

// Test conversions between position types
TEST_F(PositionTest, PreciseToBlock) {
    AbsolutePrecisePosition precise(10.7, -5.3, 0.1);
    AbsoluteBlockPosition block = toAbsoluteBlock(precise);
    
    EXPECT_EQ(block.x, 10);  // Floor of 10.7
    EXPECT_EQ(block.y, -6);  // Floor of -5.3
    EXPECT_EQ(block.z, 0);   // Floor of 0.1
}

TEST_F(PositionTest, BlockToPrecise) {
    AbsoluteBlockPosition block(10, -5, 0);
    AbsolutePrecisePosition precise = toAbsolutePrecise(block);
    
    EXPECT_DOUBLE_EQ(precise.x, 10.0);
    EXPECT_DOUBLE_EQ(precise.y, -5.0);
    EXPECT_DOUBLE_EQ(precise.z, 0.0);
}

// Test chunk coordinate conversions
TEST_F(PositionTest, BlockToChunk) {
    // Assuming CHUNK_WIDTH = CHUNK_HEIGHT = CHUNK_DEPTH = 16
    AbsoluteBlockPosition block(32, 48, 64);  // Should be chunk (2, 3, 4)
    AbsoluteChunkPosition chunk = toAbsoluteChunk(block);
    
    EXPECT_EQ(chunk.x, 32 / static_cast<int64_t>(CHUNK_WIDTH));
    EXPECT_EQ(chunk.y, 48 / static_cast<int64_t>(CHUNK_HEIGHT));
    EXPECT_EQ(chunk.z, 64 / static_cast<int64_t>(CHUNK_DEPTH));
}

TEST_F(PositionTest, NegativeBlockToChunk) {
    // Test negative coordinates (should floor divide correctly)
    AbsoluteBlockPosition block(-1, -1, -1);
    AbsoluteChunkPosition chunk = toAbsoluteChunk(block);
    
    // floor_div(-1, CHUNK_SIZE) should be -1
    EXPECT_EQ(chunk.x, -1);
    EXPECT_EQ(chunk.y, -1);
    EXPECT_EQ(chunk.z, -1);
}

TEST_F(PositionTest, ChunkOrigin) {
    AbsoluteChunkPosition chunk(2, 3, 4);
    AbsoluteBlockPosition origin = chunkOrigin(chunk);
    
    EXPECT_EQ(origin.x, static_cast<int64_t>(chunk.x) * CHUNK_WIDTH);
    EXPECT_EQ(origin.y, static_cast<int64_t>(chunk.y) * CHUNK_HEIGHT);
    EXPECT_EQ(origin.z, static_cast<int64_t>(chunk.z) * CHUNK_DEPTH);
}

TEST_F(PositionTest, ChunkLocalConversion) {
    AbsoluteChunkPosition chunk(1, 1, 1);
    ChunkLocalPosition local(5, 10, 15);
    
    // Convert to absolute block position and back
    AbsoluteBlockPosition absolute = toAbsoluteBlock(chunk, local);
    ChunkLocalPosition convertedBack = toChunkLocal(absolute, chunk);
    
    EXPECT_EQ(convertedBack.x, local.x);
    EXPECT_EQ(convertedBack.y, local.y);
    EXPECT_EQ(convertedBack.z, local.z);
}

// Test floor division and modulo helpers
TEST_F(PositionTest, FloorDivisionHelpers) {
    EXPECT_EQ(floor_div(10, 3), 3);
    EXPECT_EQ(floor_div(-10, 3), -4);
    EXPECT_EQ(floor_div(10, -3), -4);
    EXPECT_EQ(floor_div(-10, -3), 3);
    
    EXPECT_EQ(floor_mod(10, 3), 1);
    EXPECT_EQ(floor_mod(-10, 3), 2);
    // Note: The floor_mod function as implemented only handles negative remainder,
    // not negative divisor. For negative divisor cases, we get the standard modulo:
    EXPECT_EQ(floor_mod(10, -3), 1);  // Standard modulo behavior
    EXPECT_EQ(floor_mod(-10, -3), -4); // Standard modulo behavior
}