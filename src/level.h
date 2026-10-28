#pragma once
#include <nlohmann/json.hpp>
#include <raylib.h>
#include <string>
#include <vector>

struct Tile {
        Vector2 world_pos;
        Vector2 src_pos;
};

struct TileBounds {
        int left, right, top, bottom;
};

struct Door {
        Rectangle hitbox;

        std::string target_level;
        Vector2 target_pos;
};

struct Level {
        std::string id;
        Vector2 world_pos = {0, 0};
        Vector2 world_size = {0, 0};

        std::vector<Tile> tiles = {};
        std::vector<int> collision_grid = {};

        int collision_width = 0;
        int collision_height = 0;

        // Entities
        Vector2 player_start;
        Vector2 key;
        std::vector<Door> doors;
};

struct World {
        std::vector<Level> levels = {};
        Level *current_level = nullptr;
};

Level LoadLevel(const nlohmann::json &data);

// Helper functions
TileBounds GetTileBounds(Vector2 pos);
int IsColliding(const Level *level, int x, int y);
