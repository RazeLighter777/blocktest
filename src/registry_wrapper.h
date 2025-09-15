#pragma once

#include <entt/entt.hpp>
#include "name_component.h"
#include "position.h"

// A tiny wrapper so client and server can share logic for applying snapshots
// without exposing raw entt::registry everywhere.
class GameRegistry {
public:
    entt::entity create() { return reg_.create(); }
    void destroy(entt::entity e) { if (reg_.valid(e)) reg_.destroy(e); }
    entt::registry &raw() { return reg_; }
    const entt::registry &raw() const { return reg_; }

private:
    entt::registry reg_;
};
