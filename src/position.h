#pragma once
#include <cstdint>

template<typename C>
struct Position {
    Position(C x, C y, C z) : x(x), y(y), z(z) {};
    Position(const Position& other) : x(other.x), y(other.y), z(other.z) {};
    Position() = default;
    ~Position() = default;

    const C x = 0;
    const C y = 0;
    const C z = 0;

    template<typename T>
    const Position operator+(const T& other) const {
        return Position(x + other.x, y + other.y, z + other.z);
    }
    template<typename T>
    const Position operator-(const T& other) const {
        return Position(x - other.x, y - other.y, z - other.z);
    }
    template<typename T>
    const Position operator*(const T& other) const {
        return Position(x * other.x, y * other.y, z * other.z);
    }
    template<typename T>
    const Position operator/(const T& other) const {
        return Position(x / other.x, y / other.y, z / other.z);
    }
    template<typename T>
    const Position operator%(const T& other) const {
        return Position(x % other.x, y % other.y, z % other.z);
    }
};

typedef Position<int64_t> BlockPosition;
typedef Position<float> PrecisePosition;
typedef Position<int32_t> ChunkPosition;
typedef Position<uint8_t> ChunkLocalPosition;
