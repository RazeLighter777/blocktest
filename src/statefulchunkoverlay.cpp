#include "statefulchunkoverlay.h"

#include <algorithm>
#include <cstring>
#include <type_traits>

namespace {
constexpr uint8_t kVersion = 1;
constexpr uint8_t kReserved = 0;
constexpr char kMagic[4] = {'S','C','O','1'};

inline void write_u16_le(std::vector<uint8_t>& out, uint16_t v) {
    out.push_back(static_cast<uint8_t>(v & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}
inline void write_u32_le(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back(static_cast<uint8_t>(v & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
}
inline uint16_t read_u16_le(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) |
           static_cast<uint16_t>(p[1]) << 8;
}
inline uint32_t read_u32_le(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

template <typename T>
inline bool is_all_zero(const T& obj) {
    static_assert(std::is_trivially_copyable<T>::value,
                  "Block must be trivially copyable for raw-byte serialization.");
    const auto* p = reinterpret_cast<const uint8_t*>(&obj);
    for (size_t i = 0; i < sizeof(T); ++i) {
        if (p[i] != 0) return false;
    }
    return true;
}
} // namespace

Block StatefulChunkOverlay::getBlock(const ChunkLocalPosition pos) const {
    const auto it = blocks_.find(packKey(pos));
    if (it == blocks_.end()) {
        return Block{}; // Assume zero-initialized Block is Air
    }
    return it->second;
}

void StatefulChunkOverlay::setBlock(const ChunkLocalPosition pos, Block block) {
    const uint32_t key = packKey(pos);
    if (is_all_zero(block)) {
        blocks_.erase(key); // Air: remove entry
    } else {
        blocks_[key] = block;
    }
}

std::vector<uint8_t> StatefulChunkOverlay::serialize() const {
    // Stable ordering for deterministic output
    std::vector<uint32_t> keys;
    keys.reserve(blocks_.size());
    for (const auto& kv : blocks_) keys.push_back(kv.first);
    std::sort(keys.begin(), keys.end());

    const uint16_t blockSize = static_cast<uint16_t>(sizeof(Block));
    const uint32_t count = static_cast<uint32_t>(keys.size());

    // Header: magic(4) + version(1) + reserved(1) + blockSize(2) + count(4)
    const size_t headerSize = 4 + 1 + 1 + 2 + 4;
    const size_t entrySize = 4 + blockSize;
    std::vector<uint8_t> out;
    out.reserve(headerSize + entrySize * count);

    // Magic
    out.insert(out.end(), kMagic, kMagic + 4);
    // Version + reserved
    out.push_back(kVersion);
    out.push_back(kReserved);
    // Block size and entry count
    write_u16_le(out, blockSize);
    write_u32_le(out, count);

    // Entries
    for (uint32_t key : keys) {
        auto it = blocks_.find(key);
        if (it == blocks_.end()) continue;
        // Skip Air entries defensively if present (shouldn't be)
        if (is_all_zero(it->second)) continue;

        write_u32_le(out, key);
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&it->second);
        out.insert(out.end(), bytes, bytes + blockSize);
    }

    return out;
}

std::optional<StatefulChunkOverlay> StatefulChunkOverlay::deserialize(const std::vector<uint8_t>& data) {
    // Validate header
    const size_t headerSize = 4 + 1 + 1 + 2 + 4;
    if (data.size() < headerSize) return std::nullopt;

    if (!(data[0] == static_cast<uint8_t>(kMagic[0]) &&
          data[1] == static_cast<uint8_t>(kMagic[1]) &&
          data[2] == static_cast<uint8_t>(kMagic[2]) &&
          data[3] == static_cast<uint8_t>(kMagic[3]))) {
        return std::nullopt;
    }

    const uint8_t version = data[4];
    if (version != kVersion) return std::nullopt;

    const uint16_t blockSize = read_u16_le(&data[6]);
    if (blockSize != sizeof(Block)) {
        // Incompatible Block binary size
        return std::nullopt;
    }

    const uint32_t count = read_u32_le(&data[8]);

    const size_t entrySize = 4 + blockSize;
    const size_t expectedSize = headerSize + static_cast<size_t>(count) * entrySize;
    if (data.size() != expectedSize) {
        return std::nullopt;
    }

    StatefulChunkOverlay overlay;
    overlay.blocks_.reserve(count);

    size_t offset = headerSize;
    for (uint32_t i = 0; i < count; ++i) {
        const uint32_t key = read_u32_le(&data[offset]);
        offset += 4;

        Block block{};
        std::memcpy(&block, &data[offset], blockSize);
        offset += blockSize;

        // Skip Air entries to keep the map sparse
        if (!is_all_zero(block)) {
            overlay.blocks_.emplace(key, block);
        }
    }

    return overlay;
}

StatefulChunkOverlay::StatefulChunkOverlay(const ChunkOverlay& other) {
    // Populate from another overlay by checking all positions
    for (uint8_t x = 0; x < CHUNK_WIDTH; ++x) {
        for (uint8_t y = 0; y < CHUNK_HEIGHT; ++y) {
            for (uint8_t z = 0; z < CHUNK_DEPTH; ++z) {
                ChunkLocalPosition pos(x, y, z);
                Block block = other.getBlock(pos);
                if (!is_all_zero(block)) {
                    blocks_[packKey(pos)] = block;
                }
            }
        }
    }
}