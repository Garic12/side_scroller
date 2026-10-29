#include "level.h"
#include "globals.h"
#include "raylib.h"
#include <cstdio>

Level LoadLevel(const nlohmann::json &data) {
    Level level;

    level.id = data["identifier"].get<std::string>();
    level.world_pos = {data["worldX"].get<float>(),
                       data["worldY"].get<float>()};
    level.world_size = {data["pxWid"].get<float>(), data["pxHei"].get<float>()};

    for (const auto &layer : data["layerInstances"]) {
        const std::string type = layer["__type"].get<std::string>();

        if (type == "Entities") {
            const auto &entities = layer["entityInstances"];

            for (const auto &entity : entities) {
                // TODO: Also feed in other entities
                const std::string entity_id =
                    entity["__identifier"].get<std::string>();

                if (entity_id == "PlayerStart") {
                    const float x = entity["px"][0].get<float>();
                    const float y = entity["px"][1].get<float>();
                    level.player_start = {level.world_pos.x + x,
                                          level.world_pos.y + y};

                } else if (entity_id == "Door") {
                    Door door;

                    Vector2 pos = {entity["__worldX"].get<float>(),
                                   entity["__worldY"].get<float>()};
                    Vector2 size = {entity["width"].get<float>(),
                                    entity["height"].get<float>()};
                    door.hitbox = {pos.x, pos.y, size.x, size.y};

                    // Get target location of door
                    const float target_x =
                        entity["fieldInstances"][0]["__value"][0].get<float>();
                    const float target_y =
                        entity["fieldInstances"][0]["__value"][1].get<float>();
                    door.target_pos = {target_x, target_y};

                    // Get target level name
                    door.target_level = entity["fieldInstances"][1]["__value"]
                                            .get<std::string>();
                    level.doors.push_back(door);
                } else if (entity_id == "Spike") {
                    Vector2 spike_pos = {entity["__worldX"].get<float>(),
                                         entity["__worldY"].get<float>()};
                    Spike spike;
                    spike.a = {spike_pos.x + 1, spike_pos.y + 7};
                    spike.b = {spike_pos.x + 4, spike_pos.y + 1};
                    spike.c = {spike_pos.x + 7, spike_pos.y + 7};
                    printf("%.2f, %.2f\n", spike.a.x, spike.a.y);

                    level.spikes.push_back(spike);
                }
            }
        } else if (type == "Tiles") {
            const auto &grid_tiles = layer["gridTiles"];

            level.tiles.reserve(grid_tiles.size());

            for (const auto &tile : grid_tiles) {
                level.tiles.push_back(
                    {.world_pos = {level.world_pos.x +
                                       tile["px"][0].get<float>(),
                                   level.world_pos.y +
                                       tile["px"][1].get<float>()},
                     .src_pos = {tile["src"][0], tile["src"][1]}});
            }
        } else if (type == "IntGrid") {
            // Build collision grid
            int width = layer["__cWid"];
            int height = layer["__cHei"];
            level.collision_width = width;
            level.collision_height = height;

            level.collision_grid.resize(width * height);

            for (int i = 0; i < width * height; i++) {
                level.collision_grid[i] = layer["intGridCsv"][i];
            }
        }
    }
    return level;
}

// Helper functions
TileBounds GetTileBounds(const Rectangle &rect) {
    // Returns surrounding tiles in gridspace
    return {(int)floor(rect.x / TILESIZE),
            (int)floor((rect.x + rect.width - 0.01f) / TILESIZE),
            (int)floor(rect.y / TILESIZE),
            (int)floor((rect.y + rect.height - 0.01f) / TILESIZE)};
}

int IsColliding(const Level *level, int x, int y) {
    // x, y are world tile coords, convert to level-local grid coords
    x -= (int)(level->world_pos.x / TILESIZE);
    y -= (int)(level->world_pos.y / TILESIZE);
    if (x < 0 || x >= level->collision_width || y < 0 ||
        y >= level->collision_height)
        return false;
    return level->collision_grid[y * level->collision_width + x];
}
