#pragma once

#include <string>

/**
 * @brief Name component for entities, typically used for players.
 * 
 * This component holds a string name that can be assigned to any entity
 * in the entt registry. Commonly used for player identification.
 */
struct NameComponent {
    std::string name;
    
    NameComponent() = default;
    explicit NameComponent(const std::string& playerName) : name(playerName) {}
    explicit NameComponent(std::string&& playerName) : name(std::move(playerName)) {}
};