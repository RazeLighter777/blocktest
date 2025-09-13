#pragma once
#include <cstdint>
#include <cstddef>
#include <tuple>
enum class Block : uint8_t {
    Empty = 0,
    Air = 1,
    Grass = 2,
    Stone = 3,
    Water = 4,
    Sand = 5,
    Wood = 6,
    Leaves = 7,
    Bedrock = 8,
    Dirt = 9,
    // Add more block types as needed
};

constexpr size_t TEXTURE_ATLAS_WIDTH = 512;
constexpr size_t TEXTURE_ATLAS_HEIGHT = 512;

constexpr size_t BLOCK_TEXTURE_SIZE = 16; // Each block texture is 16x16 pixels
constexpr size_t BLOCKS_PER_ROW = TEXTURE_ATLAS_WIDTH / BLOCK_TEXTURE_SIZE;

[[maybe_unused]] constexpr std::tuple<size_t, size_t> getTextureIndex(Block block) {
    switch (block) {
        case Block::Empty:
        case Block::Air:   return {30, 17}; // Transparent texture
        case Block::Grass: return {4, 10};
        case Block::Stone: return {19, 6};
        case Block::Wood:  return {5, 1};
        case Block::Dirt:  return {3, 0};
        case Block::Bedrock: return {4, 3};
        default:          return {0, 0}; // Default to first texture
    }
}

[[maybe_unused]] constexpr std::tuple<size_t, size_t> getTextureCoords(Block block) {
    auto [texX, texY] = getTextureIndex(block);
    size_t x = static_cast<size_t>(texX * BLOCK_TEXTURE_SIZE);
    size_t y = static_cast<size_t>(texY * BLOCK_TEXTURE_SIZE);
    return {x, y};
}
