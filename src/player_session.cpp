#include "player_session.h"
#include <random>
#include <sstream>
#include <iomanip>

std::string PlayerSessionManager::generateSessionToken() {
    // Generate a random session token
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;
    
    uint64_t token1 = dis(gen);
    uint64_t token2 = dis(gen);
    
    std::stringstream ss;
    ss << std::hex << token1 << token2;
    return ss.str();
}

std::string PlayerSessionManager::createSession(const std::string& playerName, 
                                               entt::entity playerEntity, 
                                               const AbsolutePrecisePosition& position) {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    
    std::string token = generateSessionToken();
    
    // Ensure token is unique (very unlikely collision, but be safe)
    while (activeSessions_.find(token) != activeSessions_.end()) {
        token = generateSessionToken();
    }
    
    activeSessions_.emplace(token, PlayerSession(token, playerName, playerEntity, position));
    return token;
}

bool PlayerSessionManager::refreshSession(const std::string& sessionToken) {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    
    auto it = activeSessions_.find(sessionToken);
    if (it != activeSessions_.end()) {
        it->second.lastRefresh = std::chrono::steady_clock::now();
        return true;
    }
    return false;
}

bool PlayerSessionManager::updatePlayerPosition(const std::string& sessionToken, 
                                               const AbsolutePrecisePosition& position) {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    
    auto it = activeSessions_.find(sessionToken);
    if (it != activeSessions_.end()) {
        it->second.position = position;
        it->second.lastRefresh = std::chrono::steady_clock::now(); // Also refresh on position update
        return true;
    }
    return false;
}

bool PlayerSessionManager::isValidSession(const std::string& sessionToken) const {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    
    auto it = activeSessions_.find(sessionToken);
    if (it == activeSessions_.end()) {
        return false;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = now - it->second.lastRefresh;
    return elapsed < SESSION_TIMEOUT;
}

// Internal helper method that doesn't acquire the lock (assumes lock is already held)
bool PlayerSessionManager::isValidSessionInternal(const std::string& sessionToken) const {
    auto it = activeSessions_.find(sessionToken);
    if (it == activeSessions_.end()) {
        return false;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = now - it->second.lastRefresh;
    return elapsed < SESSION_TIMEOUT;
}

std::optional<PlayerSession> PlayerSessionManager::getSession(const std::string& sessionToken) const {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    
    auto it = activeSessions_.find(sessionToken);
    if (it != activeSessions_.end() && isValidSessionInternal(sessionToken)) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<std::string> PlayerSessionManager::removeExpiredSessions() {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    std::vector<std::string> expiredTokens;
    
    auto now = std::chrono::steady_clock::now();
    
    for (auto it = activeSessions_.begin(); it != activeSessions_.end();) {
        auto elapsed = now - it->second.lastRefresh;
        if (elapsed >= SESSION_TIMEOUT) {
            expiredTokens.push_back(it->first);
            it = activeSessions_.erase(it);
        } else {
            ++it;
        }
    }
    
    return expiredTokens;
}

void PlayerSessionManager::removeSession(const std::string& sessionToken) {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    activeSessions_.erase(sessionToken);
}

std::vector<PlayerSession> PlayerSessionManager::getAllActiveSessions() const {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    std::vector<PlayerSession> sessions;
    
    auto now = std::chrono::steady_clock::now();
    
    for (const auto& [token, session] : activeSessions_) {
        auto elapsed = now - session.lastRefresh;
        if (elapsed < SESSION_TIMEOUT) {
            sessions.push_back(session);
        }
    }
    
    return sessions;
}

size_t PlayerSessionManager::getActiveSessionCount() const {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    
    auto now = std::chrono::steady_clock::now();
    size_t count = 0;
    
    for (const auto& [token, session] : activeSessions_) {
        auto elapsed = now - session.lastRefresh;
        if (elapsed < SESSION_TIMEOUT) {
            count++;
        }
    }
    
    return count;
}