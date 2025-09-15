#pragma once
#include "position.h"
#include "block.h"
#include "chunkdims.h"
#include <cstdint>
#include <array>
#include <vector>

typedef std::vector<uint8_t> ChunkSerializationSparseVector;
struct ChunkSpan {
public:
    std::array<Block, CHUNK_WIDTH * CHUNK_HEIGHT * CHUNK_DEPTH> storage; // Internal storage for the chunk data
    const std::uint32_t strideY = CHUNK_WIDTH;                   // distance between consecutive y elements
    const std::uint32_t strideZ = CHUNK_WIDTH * CHUNK_HEIGHT;    // distance between consecutive z slices
    const AbsoluteChunkPosition position{0,0,0}; // Optional: position of this chunk in world space
    ChunkSpan() = delete; // Prevent default constructor
    ChunkSpan(AbsoluteChunkPosition pos) : position(pos) {}
    ChunkSpan(std::array<Block, CHUNK_WIDTH * CHUNK_HEIGHT * CHUNK_DEPTH> storage, AbsoluteChunkPosition pos = {0,0,0})
        : storage(storage), position(pos) {};
    ChunkSpan(ChunkSerializationSparseVector& serializedData);
    ChunkSpan(ChunkSpan& other) = default;
    ChunkSpan(const ChunkSpan& other) = default;
    ~ChunkSpan() = default;
    ChunkSpan(ChunkSpan&& other) = default;
    ChunkSpan& operator=(const ChunkSpan& other) = default;
    ChunkSpan& operator=(ChunkSpan&& other) = default;
    ChunkSerializationSparseVector serialize() const;
    
    // Access block at local coordinates within the chunk
    Block getBlock(const ChunkLocalPosition& localPos) const;
    void setBlock(const ChunkLocalPosition& localPos, Block block);

};