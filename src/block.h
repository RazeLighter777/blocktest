#pragma once
#include <cstdint>
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