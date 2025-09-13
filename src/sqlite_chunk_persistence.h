#pragma once

#include "world.h"
#include "statefulchunkoverlay.h"
#include <sqlite3.h>
#include <memory>
#include <iostream>

class SQLiteChunkPersistence : public IChunkPersistence {
public:
    explicit SQLiteChunkPersistence(std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db);
    ~SQLiteChunkPersistence() override = default;

    bool saveChunk(const AbsoluteChunkPosition& pos, const ChunkSpan& chunk) override;
    std::optional<std::shared_ptr<ChunkSpan>> loadChunk(const AbsoluteChunkPosition& pos) override;
    void saveAllLoadedChunks(const ChunkMap& chunks) override;

private:
    void initializeDatabase();
    std::shared_ptr<sqlite3> db_;
};