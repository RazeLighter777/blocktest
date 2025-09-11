#include "emptychunk.h"

Block EmptyChunk::getBlock([[maybe_unused]] const ChunkLocalPosition pos) const {
    return Block::Empty;
}
