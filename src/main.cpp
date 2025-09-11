#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <GLFW/glfw3.h>
#include "block.h"
#include "chunkoverlay.h"
#include "position.h"
#include "emptychunk.h"
int main(void) {
    printf("Block Test Application\n");
    auto emptyChunk1 = std::make_shared<EmptyChunk>();
    auto emptyChunk2 = std::make_shared<EmptyChunk>();
    auto combinedChunk = emptyChunk1 & emptyChunk2;
    ChunkLocalPosition pos(0, 0, 0);
    Block block = combinedChunk->getBlock(pos);
    if (block == Block::Empty) {
        printf("Block at position (0,0,0) is Empty as expected.\n");
    } else {
        printf("Unexpected block type at position (0,0,0).\n");
    }

    return EXIT_SUCCESS;
}