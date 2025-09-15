
#include "chunkspan.h"
#include <stdexcept>
#include <cstring>

// Version tag for serialization
constexpr uint8_t CHUNKSPAN_SPARSE_SERIALIZATION_VERSION = 1;

// Helper: get total block count
constexpr size_t CHUNK_BLOCK_COUNT = CHUNK_WIDTH * CHUNK_HEIGHT * CHUNK_DEPTH;

ChunkSerializationSparseVector ChunkSpan::serialize() const {
	ChunkSerializationSparseVector out;
	// Version
	out.push_back(CHUNKSPAN_SPARSE_SERIALIZATION_VERSION);
	// Position (x, y, z as int32_t)
	for (int i = 0; i < 3; ++i) {
		int32_t coord = (i == 0) ? position.x : (i == 1) ? position.y : position.z;
		uint8_t* p = reinterpret_cast<uint8_t*>(&coord);
		out.insert(out.end(), p, p + sizeof(int32_t));
	}
	// Count non-empty blocks
	uint32_t nonempty_count = 0;
	for (size_t i = 0; i < storage.size(); ++i) {
		if (storage[i] != Block::Empty) ++nonempty_count;
	}
	// Write count (uint32_t)
	uint8_t* pcount = reinterpret_cast<uint8_t*>(&nonempty_count);
	out.insert(out.end(), pcount, pcount + sizeof(uint32_t));
	// Write (index, value) for each non-empty block
	for (size_t i = 0; i < storage.size(); ++i) {
		if (storage[i] != Block::Empty) {
			uint32_t idx = static_cast<uint32_t>(i);
			uint8_t* pidx = reinterpret_cast<uint8_t*>(&idx);
			out.insert(out.end(), pidx, pidx + sizeof(uint32_t));
			out.push_back(static_cast<uint8_t>(storage[i]));
		}
	}
	return out;
}

ChunkSpan::ChunkSpan(ChunkSerializationSparseVector& serializedData)
	: storage{}, position{0,0,0}
{
	size_t offset = 0;
	// Version
	if (serializedData.size() < 1) throw std::runtime_error("Serialized data too short");
	uint8_t version = serializedData[offset++];
	if (version != CHUNKSPAN_SPARSE_SERIALIZATION_VERSION) throw std::runtime_error("Unknown ChunkSpan serialization version");
	// Position
	if (serializedData.size() < offset + 3 * sizeof(int32_t)) throw std::runtime_error("Serialized data too short for position");
	int32_t pos[3];
	for (int i = 0; i < 3; ++i) {
		std::memcpy(&pos[i], &serializedData[offset], sizeof(int32_t));
		offset += sizeof(int32_t);
	}
	const_cast<AbsoluteChunkPosition&>(position) = AbsoluteChunkPosition(pos[0], pos[1], pos[2]);
	// Non-empty count
	if (serializedData.size() < offset + sizeof(uint32_t)) throw std::runtime_error("Serialized data too short for count");
	uint32_t nonempty_count = 0;
	std::memcpy(&nonempty_count, &serializedData[offset], sizeof(uint32_t));
	offset += sizeof(uint32_t);
	// Fill storage with Empty
	storage.fill(Block::Empty);
	// Read (index, value) pairs
	for (uint32_t i = 0; i < nonempty_count; ++i) {
		if (serializedData.size() < offset + sizeof(uint32_t) + 1) throw std::runtime_error("Serialized data too short for block entry");
		uint32_t idx = 0;
		std::memcpy(&idx, &serializedData[offset], sizeof(uint32_t));
		offset += sizeof(uint32_t);
		uint8_t val = serializedData[offset++];
		if (idx >= storage.size()) throw std::runtime_error("Block index out of range");
		storage[idx] = static_cast<Block>(val);
	}
}

Block ChunkSpan::getBlock(const ChunkLocalPosition& localPos) const {
    size_t index = localPos.x + localPos.y * strideY + localPos.z * strideZ;
    return storage[index];
}

void ChunkSpan::setBlock(const ChunkLocalPosition& localPos, Block block) {
    size_t index = localPos.x + localPos.y * strideY + localPos.z * strideZ;
    storage[index] = block;
}
