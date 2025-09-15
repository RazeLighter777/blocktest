#include <gtest/gtest.h>
#include "block.h"

class BlockTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code if needed
    }
    
    void TearDown() override {
        // Cleanup code if needed
    }
};

// Test basic block enum values
TEST_F(BlockTest, BlockEnumValues) {
    EXPECT_EQ(static_cast<uint8_t>(Block::Empty), 0);
    EXPECT_EQ(static_cast<uint8_t>(Block::Air), 1);
    EXPECT_EQ(static_cast<uint8_t>(Block::Grass), 2);
    EXPECT_EQ(static_cast<uint8_t>(Block::Stone), 3);
    EXPECT_EQ(static_cast<uint8_t>(Block::Water), 4);
    EXPECT_EQ(static_cast<uint8_t>(Block::Sand), 5);
    EXPECT_EQ(static_cast<uint8_t>(Block::Wood), 6);
    EXPECT_EQ(static_cast<uint8_t>(Block::Leaves), 7);
    EXPECT_EQ(static_cast<uint8_t>(Block::Bedrock), 8);
    EXPECT_EQ(static_cast<uint8_t>(Block::Dirt), 9);
}

// Test texture coordinates calculation
TEST_F(BlockTest, TextureCoordinates) {
    auto [grassTexX, grassTexY] = getTextureCoords(Block::Grass);
    auto [stoneTexX, stoneTexY] = getTextureCoords(Block::Stone);
    auto [woodTexX, woodTexY] = getTextureCoords(Block::Wood);
    
    // Verify that texture coordinates are within reasonable bounds
    EXPECT_LT(grassTexX, TEXTURE_ATLAS_WIDTH);
    EXPECT_LT(grassTexY, TEXTURE_ATLAS_HEIGHT);
    EXPECT_LT(stoneTexX, TEXTURE_ATLAS_WIDTH);
    EXPECT_LT(stoneTexY, TEXTURE_ATLAS_HEIGHT);
    EXPECT_LT(woodTexX, TEXTURE_ATLAS_WIDTH);
    EXPECT_LT(woodTexY, TEXTURE_ATLAS_HEIGHT);
}

// Test texture index calculation
TEST_F(BlockTest, TextureIndices) {
    auto [grassIndexX, grassIndexY] = getTextureIndex(Block::Grass);
    auto [stoneIndexX, stoneIndexY] = getTextureIndex(Block::Stone);
    auto [emptyIndexX, emptyIndexY] = getTextureIndex(Block::Empty);
    auto [airIndexX, airIndexY] = getTextureIndex(Block::Air);
    
    // Verify specific expected values from the function
    EXPECT_EQ(grassIndexX, 4);
    EXPECT_EQ(grassIndexY, 10);
    EXPECT_EQ(stoneIndexX, 19);
    EXPECT_EQ(stoneIndexY, 6);
    
    // Empty and Air should have the same texture (transparent)
    EXPECT_EQ(emptyIndexX, airIndexX);
    EXPECT_EQ(emptyIndexY, airIndexY);
    EXPECT_EQ(emptyIndexX, 30);
    EXPECT_EQ(emptyIndexY, 17);
}

// Test constants
TEST_F(BlockTest, Constants) {
    EXPECT_EQ(TEXTURE_ATLAS_WIDTH, 512);
    EXPECT_EQ(TEXTURE_ATLAS_HEIGHT, 512);
    EXPECT_EQ(BLOCK_TEXTURE_SIZE, 16);
    EXPECT_EQ(BLOCKS_PER_ROW, TEXTURE_ATLAS_WIDTH / BLOCK_TEXTURE_SIZE);
    EXPECT_EQ(BLOCKS_PER_ROW, 32);
}