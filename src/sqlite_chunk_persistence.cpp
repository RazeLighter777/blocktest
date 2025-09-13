#include "sqlite_chunk_persistence.h"
#include "statefulchunkoverlay.h"
#include "chunkdims.h"
#include "block.h"
#include <iostream>

namespace {
// A ChunkSpan that owns its storage so data remains valid while referenced.
struct OwningChunkSpan : public ChunkSpan {
    std::vector<Block> storage;
    explicit OwningChunkSpan(const AbsoluteChunkPosition pos)
        : ChunkSpan(pos), storage(kChunkElemCount, Block::Empty) {
        data = storage.data();
        // Keep default strides from base: strideY=CHUNK_WIDTH, strideZ=CHUNK_WIDTH*CHUNK_HEIGHT
    }
};

// A ChunkSpan that can be persisted using StatefulChunkOverlay
struct PersistentChunkSpan : public OwningChunkSpan {
    StatefulChunkOverlay overlay;
    
    explicit PersistentChunkSpan(const AbsoluteChunkPosition pos) 
        : OwningChunkSpan(pos) {}
        
    explicit PersistentChunkSpan(const AbsoluteChunkPosition pos, const StatefulChunkOverlay& overlay)
        : OwningChunkSpan(pos), overlay(overlay) {
        // Generate the chunk data from the overlay
        overlay.generate(*this);
    }
};
}

SQLiteChunkPersistence::SQLiteChunkPersistence(std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db)
    : db_(db ? std::shared_ptr<sqlite3>(db.release(), sqlite3_close) : nullptr) {
    initializeDatabase();
}

void SQLiteChunkPersistence::initializeDatabase() {
    if (!db_) return;

    const char* createTableSQL = R"sql(
        CREATE TABLE IF NOT EXISTS chunks (
            x INTEGER NOT NULL,
            y INTEGER NOT NULL,
            z INTEGER NOT NULL,
            data BLOB NOT NULL,
            PRIMARY KEY (x, y, z)
        );
    )sql";

    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_.get(), createTableSQL, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to initialize database: " << errMsg << std::endl;
        sqlite3_free(errMsg);
    }
}

bool SQLiteChunkPersistence::saveChunk(const AbsoluteChunkPosition& pos, const ChunkSpan& chunk) {
    if (!db_) {
        std::cerr << "No database connection for saving chunk\n";
        return false;
    }
    
    // Create a StatefulChunkOverlay from the chunk data
    StatefulChunkOverlay overlay;
    for (uint32_t z = 0; z < CHUNK_DEPTH; ++z) {
        for (uint32_t y = 0; y < CHUNK_HEIGHT; ++y) {
            for (uint32_t x = 0; x < CHUNK_WIDTH; ++x) {
                const std::size_t idx = static_cast<std::size_t>(z) * chunk.strideZ + 
                                      static_cast<std::size_t>(y) * chunk.strideY + x;
                const Block block = chunk.data[idx];
                if (block != Block::Empty) {
                    overlay.setBlock(ChunkLocalPosition(x, y, z), block);
                }
            }
        }
    }
    
    // Serialize the overlay
    std::vector<uint8_t> serializedData = overlay.serialize();
    std::cerr << "Saving chunk (" << pos.x << "," << pos.y << "," << pos.z << ") with " 
              << serializedData.size() << " bytes\n";
    
    // Prepare SQL statement
    const char* sql = R"sql(
        INSERT OR REPLACE INTO chunks (x, y, z, data) 
        VALUES (?, ?, ?, ?)
    )sql";
    
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_.get(), sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare save statement: " << sqlite3_errmsg(db_.get()) << "\n";
        return false;
    }
    
    // Bind parameters
    sqlite3_bind_int(stmt, 1, pos.x);
    sqlite3_bind_int(stmt, 2, pos.y);
    sqlite3_bind_int(stmt, 3, pos.z);
    sqlite3_bind_blob(stmt, 4, serializedData.data(), static_cast<int>(serializedData.size()), SQLITE_STATIC);
    
    // Execute
    rc = sqlite3_step(stmt);
    bool success = (rc == SQLITE_DONE);
    
    if (!success) {
        std::cerr << "Failed to execute save statement: " << sqlite3_errmsg(db_.get()) << "\n";
    }
    
    int result = sqlite3_finalize(stmt);
    if (result != SQLITE_OK) {
        std::cerr << "Failed to finalize save statement: " << sqlite3_errmsg(db_.get()) << "\n";
        success = false;
    }
    return success;
}

std::optional<std::shared_ptr<ChunkSpan>> SQLiteChunkPersistence::loadChunk(const AbsoluteChunkPosition& pos) {
    if (!db_) {
        std::cerr << "No database connection for loading chunk\n";
        return std::nullopt;
    }
    
    const char* sql = R"sql(
        SELECT data FROM chunks WHERE x = ? AND y = ? AND z = ?
    )sql";
    
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_.get(), sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare load statement: " << sqlite3_errmsg(db_.get()) << "\n";
        return std::nullopt;
    }
    
    // Bind parameters
    sqlite3_bind_int(stmt, 1, pos.x);
    sqlite3_bind_int(stmt, 2, pos.y);
    sqlite3_bind_int(stmt, 3, pos.z);
    
    // Execute query
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        std::cerr << "No chunk found at (" << pos.x << "," << pos.y << "," << pos.z << ") - rc=" << rc << "\n";
        sqlite3_finalize(stmt);
        return std::nullopt;
    }
    
    // Get the blob data
    const void* blobData = sqlite3_column_blob(stmt, 0);
    int blobSize = sqlite3_column_bytes(stmt, 0);
    
    if (!blobData || blobSize <= 0) {
        std::cerr << "Invalid blob data: size=" << blobSize << "\n";
        sqlite3_finalize(stmt);
        return std::nullopt;
    }
    
    std::cerr << "Loading chunk (" << pos.x << "," << pos.y << "," << pos.z << ") with " 
              << blobSize << " bytes\n";
    
    // Copy the data to a vector
    std::vector<uint8_t> serializedData(
        static_cast<const uint8_t*>(blobData),
        static_cast<const uint8_t*>(blobData) + blobSize
    );
    
    sqlite3_finalize(stmt);
    
    // Deserialize the overlay
    auto overlayOpt = StatefulChunkOverlay::deserialize(serializedData);
    if (!overlayOpt.has_value()) {
        std::cerr << "Failed to deserialize chunk overlay\n";
        return std::nullopt;
    }
    
    // Create a PersistentChunkSpan with the loaded overlay
    auto chunk = std::make_shared<PersistentChunkSpan>(pos, overlayOpt.value());
    return std::static_pointer_cast<ChunkSpan>(chunk);
}

void SQLiteChunkPersistence::saveAllLoadedChunks(const ChunkMap& chunks) {
    if (!db_) return;
    
    for (const auto& kv : chunks) {
        saveChunk(kv.first, *kv.second);
    }
}