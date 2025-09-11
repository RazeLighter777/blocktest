#include "statefulchunkoverlay.h"
#include "emptychunk.h"
#include "chunkoverlay.h"
#include "bedrockoverlay.h"
#include "perlinnoise.hpp"
#include<iostream>
#include<cassert>

void testStatefulChunkOverlayDefaultIsEmpty() {
    StatefulChunkOverlay overlay;
    for (uint8_t x = 0; x < 5; ++x) {
        for (uint8_t y = 0; y < 5; ++y) {
            for (uint8_t z = 0; z < 5; ++z) {
                ChunkLocalPosition pos(x, y, z);
                assert(overlay.getBlock(pos) == Block::Empty);
            }
        }
    }
    std::cout << "testStatefulChunkOverlayDefaultIsEmpty passed.\n";
}

void testStatefulChunkOverlaySetAndGet() {
    StatefulChunkOverlay overlay;
    ChunkLocalPosition pos(1, 2, 3);
    overlay.setBlock(pos, Block::Grass);
    assert(overlay.getBlock(pos) == Block::Grass);

    // Setting to Air should remove the block
    overlay.setBlock(pos, Block::Air);
    assert(overlay.getBlock(pos) == Block::Empty);

    std::cout << "testStatefulChunkOverlaySetAndGet passed.\n";
}

void testStatefulChunkOverlaySerialization() {
    StatefulChunkOverlay overlay;
    ChunkLocalPosition pos1(1, 2, 3);
    ChunkLocalPosition pos2(4, 5, 6);
    overlay.setBlock(pos1, Block::Stone);
    overlay.setBlock(pos2, Block::Dirt);

    auto data = overlay.serialize();
    std::cout << "Serialized data size: " << data.size() << " bytes\n";
    std::cout << "Serialized data:";
    for (uint8_t byte : data) {
        std::cout << " " << std::hex << static_cast<int>(byte);
    }
    std::cout << std::dec << "\n";
    auto deserializedOpt = StatefulChunkOverlay::deserialize(data);
    assert(deserializedOpt.has_value());
    auto deserialized = deserializedOpt.value();

    assert(deserialized.getBlock(pos1) == Block::Stone);
    assert(deserialized.getBlock(pos2) == Block::Dirt);
    assert(deserialized.getBlock(ChunkLocalPosition(0, 0, 0)) == Block::Empty);

    std::cout << "testStatefulChunkOverlaySerialization passed.\n";
}

void testStatefulChunkOverlayFromOtherOverlay() {
    EmptyChunk emptyChunk;
    StatefulChunkOverlay overlay(emptyChunk);
    for (uint8_t x = 0; x < 5; ++x) {
        for (uint8_t y = 0; y < 5; ++y) {
            for (uint8_t z = 0; z < 5; ++z) {
                ChunkLocalPosition pos(x, y, z);
                assert(overlay.getBlock(pos) == Block::Empty);
            }
        }
    }
    std::cout << "testStatefulChunkOverlayFromOtherOverlay passed.\n";
}

void testEmptyChunk() {
    EmptyChunk emptyChunk;
    for (uint8_t x = 0; x < 5; ++x) {
        for (uint8_t y = 0; y < 5; ++y) {
            for (uint8_t z = 0; z < 5; ++z) {
                ChunkLocalPosition pos(x, y, z);
                assert(emptyChunk.getBlock(pos) == Block::Empty);
            }
        }
    }
    std::cout << "testEmptyChunk passed.\n";
}

void testCombinedChunkOverlay() {
    auto emptyChunk = std::make_shared<EmptyChunk>();
    auto statefulOverlay = std::make_shared<StatefulChunkOverlay>();
    ChunkLocalPosition pos(1, 1, 1);
    statefulOverlay->setBlock(pos, Block::Wood);

    auto combined = emptyChunk & statefulOverlay;
    assert(combined->getBlock(pos) == Block::Wood);
    assert(combined->getBlock(ChunkLocalPosition(0, 0, 0)) == Block::Empty);

    std::cout << "testCombinedChunkOverlay passed.\n";
}

void testBedrockOverlayBasic() {
    auto noise = std::make_shared<siv::PerlinNoise>(12345);
    auto bedrock = std::make_shared<BedrockOverlay>(noise);

    // For a small set of x,z, bottom two layers should be bedrock
    for (uint8_t x = 0; x < 5; ++x) {
        for (uint8_t z = 0; z < 5; ++z) {
            assert(bedrock->getBlock(ChunkLocalPosition(x, 0, z)) == Block::Bedrock);
            assert(bedrock->getBlock(ChunkLocalPosition(x, 1, z)) == Block::Bedrock);
            // y=3 is definitely above 3-thick cap => must be empty
            assert(bedrock->getBlock(ChunkLocalPosition(x, 3, z)) == Block::Empty);
        }
    }
    // print out some terrain to ensure variability
    for (uint8_t x = 0; x < 16; ++x) {
        for (uint8_t z = 0; z < 16; ++z) {
            uint8_t thickness = 0;
            for (uint8_t y = 0; y < 5; ++y) {
                if (bedrock->getBlock(ChunkLocalPosition(x, y, z)) == Block::Bedrock) {
                    thickness++;
                }
            }
            std::cout << (int)thickness << " ";
        }
        std::cout << "\n";
    }
    std::cout << "testBedrockOverlayBasic passed.\n";
}


int main() {
    testStatefulChunkOverlayDefaultIsEmpty();
    testStatefulChunkOverlaySetAndGet();
    testStatefulChunkOverlaySerialization();
    testStatefulChunkOverlayFromOtherOverlay();
    testEmptyChunk();
    testCombinedChunkOverlay();
    testBedrockOverlayBasic();

    std::cout << "All tests passed.\n";
    return 0;
}