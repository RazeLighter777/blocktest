#include "emptychunk.h"

Block EmptyChunk::getBlock([[maybe_unused]] const ChunkLocalPosition pos,
                           [[maybe_unused]] const Block parentLayerBlock) const {
    return Block::Empty;
}