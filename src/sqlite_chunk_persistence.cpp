#include "sqlite_chunk_persistence.h"
#include "chunkdims.h"
#include "block.h"
#include <iostream>


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

bool SQLiteChunkPersistence::saveChunk(const ChunkSpan& chunk) {
    if (!db_) {
        std::cerr << "No database connection for saving chunk\n";
        return false;
    }
    
    const AbsoluteChunkPosition& pos = chunk.position;
    auto serializedData = chunk.serialize();
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
    
    try {
        auto chunk = std::make_shared<ChunkSpan>(serializedData);
        return chunk;
    } catch (const std::exception& ex) {
        std::cerr << "Failed to deserialize chunk: " << ex.what() << "\n";
        return std::nullopt;
    }
}

void SQLiteChunkPersistence::saveAllLoadedChunks(const ChunkMap& chunks) {
    if (!db_) return;
    for (const auto& [pos, chunk] : chunks) {
        if (chunk) {
            saveChunk(*chunk);
        }
    }
}