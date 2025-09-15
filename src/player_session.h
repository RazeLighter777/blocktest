#pragma once

#include <string>
#include <chrono>
#include <unordered_map>
#include <mutex>
#include <entt/entt.hpp>
#include "position.h"

/**
 * @brief Represents an active player session
 */
struct PlayerSession {
    std::string sessionToken;
    std::string playerName;
    entt::entity playerEntity;
    std::chrono::steady_clock::time_point lastRefresh;
    AbsolutePrecisePosition position;
    
    PlayerSession(const std::string& token, const std::string& name, 
                  entt::entity entity, const AbsolutePrecisePosition& pos)
        : sessionToken(token), playerName(name), playerEntity(entity), 
          lastRefresh(std::chrono::steady_clock::now()), position(pos) {}
};

/**
 * @brief Manages player sessions with timeout handling
 */
class PlayerSessionManager {
public:
    static constexpr std::chrono::seconds SESSION_TIMEOUT{5}; // 5 second timeout
    
    // Session management
    std::string createSession(const std::string& playerName, entt::entity playerEntity, 
                             const AbsolutePrecisePosition& position);
    bool refreshSession(const std::string& sessionToken);
    bool updatePlayerPosition(const std::string& sessionToken, const AbsolutePrecisePosition& position);
    bool isValidSession(const std::string& sessionToken) const;
    std::optional<PlayerSession> getSession(const std::string& sessionToken) const;
    std::vector<std::string> removeExpiredSessions();
    void removeSession(const std::string& sessionToken);
    
    // Session queries
    std::vector<PlayerSession> getAllActiveSessions() const;
    size_t getActiveSessionCount() const;
    
private:
    std::string generateSessionToken();
    bool isValidSessionInternal(const std::string& sessionToken) const; // Internal helper without locking
    
    mutable std::mutex sessionsMutex_;
    std::unordered_map<std::string, PlayerSession> activeSessions_;
};