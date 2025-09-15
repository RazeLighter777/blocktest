#pragma once
// Position types and conversions between absolute/chunk/local coordinates.
// Chunk dimension accessors are declared here and defined in a .cpp to avoid
// circular includes with chunkoverlay.h.
#include "chunkdims.h"
#include <cstdint>
#include <cassert>
#include <cmath>
#include <limits>

template<typename C>
struct Position {
    Position(C x, C y, C z) : x(x), y(y), z(z) {};
    Position(const Position& other) : x(other.x), y(other.y), z(other.z) {};
    Position() = default;
    ~Position() = default;
    Position& operator=(const Position& other) {
        if (this != &other) {
            x = other.x;
            y = other.y;
            z = other.z;
        }
        return *this;
    }

    C x = 0;
    C y = 0;
    C z = 0;

    const Position operator+(const C& other) const {
        return Position(x + other.x, y + other.y, z + other.z);
    }
    const Position operator-(const C& other) const {
        return Position(x - other.x, y - other.y, z - other.z);
    }
    const Position operator*(const C& other) const {
        return Position(x * other.x, y * other.y, z * other.z);
    }
    const Position operator/(const C& other) const {
        return Position(x / other.x, y / other.y, z / other.z);
    }
    const Position operator%(const C& other) const {
        return Position(x % other.x, y % other.y, z % other.z);
    }
};

// Specialization for chunk-local positions (uint32_t) that asserts bounds.
template<>
struct Position<std::uint32_t> {
    Position(std::uint32_t x, std::uint32_t y, std::uint32_t z) : x(x), y(y), z(z) {
        assert_in_bounds();
    }
    Position(const Position& other) : x(other.x), y(other.y), z(other.z) {
        assert_in_bounds();
    }
    Position() = default;
    ~Position() = default;
    Position& operator=(const Position& other) {
        if (this != &other) {
            x = other.x;
            y = other.y;
            z = other.z;
            assert_in_bounds();
        }
        return *this;
    }

    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t z = 0;

    template<typename T>
    const Position operator+(const T& other) const {
        return Position(
            static_cast<std::uint32_t>(static_cast<std::uint64_t>(x) + static_cast<std::uint64_t>(other.x)),
            static_cast<std::uint32_t>(static_cast<std::uint64_t>(y) + static_cast<std::uint64_t>(other.y)),
            static_cast<std::uint32_t>(static_cast<std::uint64_t>(z) + static_cast<std::uint64_t>(other.z))
        );
    }
    template<typename T>
    const Position operator-(const T& other) const {
        return Position(
            static_cast<std::uint32_t>(static_cast<std::uint64_t>(x) - static_cast<std::uint64_t>(other.x)),
            static_cast<std::uint32_t>(static_cast<std::uint64_t>(y) - static_cast<std::uint64_t>(other.y)),
            static_cast<std::uint32_t>(static_cast<std::uint64_t>(z) - static_cast<std::uint64_t>(other.z))
        );
    }
    template<typename T>
    const Position operator*(const T& other) const {
        return Position(
            static_cast<std::uint32_t>(static_cast<std::uint64_t>(x) * static_cast<std::uint64_t>(other.x)),
            static_cast<std::uint32_t>(static_cast<std::uint64_t>(y) * static_cast<std::uint64_t>(other.y)),
            static_cast<std::uint32_t>(static_cast<std::uint64_t>(z) * static_cast<std::uint64_t>(other.z))
        );
    }
    template<typename T>
    const Position operator/(const T& other) const {
        return Position(
            static_cast<std::uint32_t>(x / static_cast<std::uint64_t>(other.x)),
            static_cast<std::uint32_t>(y / static_cast<std::uint64_t>(other.y)),
            static_cast<std::uint32_t>(z / static_cast<std::uint64_t>(other.z))
        );
    }
    template<typename T>
    const Position operator%(const T& other) const {
        return Position(
            static_cast<std::uint32_t>(x % static_cast<std::uint64_t>(other.x)),
            static_cast<std::uint32_t>(y % static_cast<std::uint64_t>(other.y)),
            static_cast<std::uint32_t>(z % static_cast<std::uint64_t>(other.z))
        );
    }

private:
    inline void assert_in_bounds() const {
        static_assert(CHUNK_WIDTH > 0 && CHUNK_HEIGHT > 0 && CHUNK_DEPTH > 0);
        assert(x < CHUNK_WIDTH);
        assert(y < CHUNK_HEIGHT);
        assert(z < CHUNK_DEPTH);
    }
};

// AbsoluteBlockPosition is for absolute block coordinates in the world.
typedef Position<int64_t> AbsoluteBlockPosition;
// AbsolutePrecisePosition is for precise coordinates within the world, e.g. for entity positions.
typedef Position<double> AbsolutePrecisePosition;
// AbsoluteChunkPosition is for chunk coordinates. To convert to absolute block coordinates, multiply by CHUNK_WIDTH,CHUNK_HEIGHT,CHUNK_DEPTH respectively.
typedef Position<int32_t> AbsoluteChunkPosition;
// Position within a single chunk. Will assert if out of bounds.
typedef Position<uint32_t> ChunkLocalPosition;

// Helpers for safe floor division/mod for integer coordinates (handle negatives).
inline constexpr std::int64_t floor_div(std::int64_t a, std::int64_t b) {
    std::int64_t q = a / b;
    std::int64_t r = a % b;
    if (r != 0 && ((r > 0) != (b > 0))) --q;
    return q;
}
inline constexpr std::int64_t floor_mod(std::int64_t a, std::int64_t b) {
    std::int64_t r = a % b;
    if (r != 0 && r < 0) r += b;
    return r;
}

// Conversions:

// Precise -> Block (floor each component)
inline AbsoluteBlockPosition toAbsoluteBlock(const AbsolutePrecisePosition& p) {
    using std::floor;
    return AbsoluteBlockPosition(
        static_cast<std::int64_t>(floor(p.x)),
        static_cast<std::int64_t>(floor(p.y)),
        static_cast<std::int64_t>(floor(p.z))
    );
}

// Block -> Precise (widen)
inline AbsolutePrecisePosition toAbsolutePrecise(const AbsoluteBlockPosition& b) {
    return AbsolutePrecisePosition(
        static_cast<double>(b.x),
        static_cast<double>(b.y),
        static_cast<double>(b.z)
    );
}

// Block -> Chunk (floor-divide by chunk dimensions)
inline AbsoluteChunkPosition toAbsoluteChunk(const AbsoluteBlockPosition& b) {
    const std::int64_t W = static_cast<std::int64_t>(CHUNK_WIDTH);
    const std::int64_t H = static_cast<std::int64_t>(CHUNK_HEIGHT);
    const std::int64_t D = static_cast<std::int64_t>(CHUNK_DEPTH);

    const std::int64_t cx = floor_div(b.x, W);
    const std::int64_t cy = floor_div(b.y, H);
    const std::int64_t cz = floor_div(b.z, D);

    assert(cx >= std::numeric_limits<std::int32_t>::min() && cx <= std::numeric_limits<std::int32_t>::max());
    assert(cy >= std::numeric_limits<std::int32_t>::min() && cy <= std::numeric_limits<std::int32_t>::max());
    assert(cz >= std::numeric_limits<std::int32_t>::min() && cz <= std::numeric_limits<std::int32_t>::max());

    return AbsoluteChunkPosition(static_cast<std::int32_t>(cx), static_cast<std::int32_t>(cy), static_cast<std::int32_t>(cz));
}

// Precise -> Chunk (via block)
inline AbsoluteChunkPosition toAbsoluteChunk(const AbsolutePrecisePosition& p) {
    return toAbsoluteChunk(toAbsoluteBlock(p));
}

// Chunk -> Block (origin of the chunk in block coordinates)
inline AbsoluteBlockPosition chunkOrigin(const AbsoluteChunkPosition& c) {

    return AbsoluteBlockPosition(
        static_cast<std::int64_t>(c.x) * CHUNK_WIDTH,
        static_cast<std::int64_t>(c.y) * CHUNK_HEIGHT,
        static_cast<std::int64_t>(c.z) * CHUNK_DEPTH
    );
}

// Chunk + ChunkLocal -> Block
inline AbsoluteBlockPosition toAbsoluteBlock(const AbsoluteChunkPosition& c, const ChunkLocalPosition& l) {
    const AbsoluteBlockPosition origin = chunkOrigin(c);
    return AbsoluteBlockPosition(
        origin.x + static_cast<std::int64_t>(l.x),
        origin.y + static_cast<std::int64_t>(l.y),
        origin.z + static_cast<std::int64_t>(l.z)
    );
}

// Block -> ChunkLocal (requires the target absolute chunk position)
inline ChunkLocalPosition toChunkLocal(const AbsoluteBlockPosition& b, const AbsoluteChunkPosition& c) {
    const AbsoluteBlockPosition origin = chunkOrigin(c);


    const std::int64_t dx = b.x - origin.x;
    const std::int64_t dy = b.y - origin.y;
    const std::int64_t dz = b.z - origin.z;

    // Assert that the block lies within the given chunk.
    assert(dx >= 0 && dx < CHUNK_WIDTH);
    assert(dy >= 0 && dy < CHUNK_HEIGHT);
    assert(dz >= 0 && dz < CHUNK_DEPTH);

    return ChunkLocalPosition(
        static_cast<std::uint32_t>(dx),
        static_cast<std::uint32_t>(dy),
        static_cast<std::uint32_t>(dz)
    );
}
