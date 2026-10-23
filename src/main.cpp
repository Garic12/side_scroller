#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <raylib.h>
#include <raymath.h>

using json = nlohmann::json;

// Constants
constexpr int screenWidth = 1000;
constexpr int screenHeight = 600;
constexpr float TILESIZE = 8.0f;

// Game Data
struct Player {
        float jump_height;
        float jump_time_to_peak;
        float jump_time_to_fall;

        float jump_velocity;
        float jump_gravity;
        float fall_gravity;

        float walk_speed;

        Vector2 pos = {0.0f, 0.0f};
        Vector2 vel = {0.0f, 0.0f};

        bool grounded = true;
        bool touches_wall;

        float wall_coyote_timer;
        float wall_coyote_time;
};

struct Tile {
        Vector2 world_pos;
        Vector2 src_pos;
};

struct TileBounds {
        int left, right, top, bottom;
};

struct Level {
        Vector2 world_pos = {0, 0};
        Vector2 world_size = {0, 0};
        std::vector<Tile> tiles = {};
        std::vector<int> collision_grid = {};
        int collision_width = 0;
        int collision_height = 0;
        std::string tileset = "";
};

int SetupPlayer(Player *player) {
    player->jump_height = 18.0f;
    player->jump_time_to_peak = 0.35f;
    player->jump_time_to_fall = 0.25f;

    player->jump_velocity =
        (2.0f * player->jump_height) / player->jump_time_to_peak;
    player->jump_gravity =
        (-2.0f * player->jump_height) /
        (player->jump_time_to_peak * player->jump_time_to_peak) * -1;
    player->fall_gravity =
        (-2.0f * player->jump_height) /
        (player->jump_time_to_fall * player->jump_time_to_fall) * -1;

    player->wall_coyote_time = 0.1f;
    player->wall_coyote_timer = 0.0f;

    player->walk_speed = 45.0f;

    return 0;
}

int LoadLevel(Level *level, Texture2D *texture, Player *player) {
    // Parse map information
    std::ifstream file("ressources/main.ldtk");
    if (!file) {
        std::cerr << "Could not open main.ldtk\n";
        return 1;
    }
    json ldtk = json::parse(file);

    const auto map = ldtk["levels"][0];
    level->world_pos = {map["worldX"].get<float>(), map["worldY"].get<float>()};
    level->world_size = {map["pxWid"].get<float>(), map["pxHei"].get<float>()};

    for (const auto &layer : map["layerInstances"]) {
        const std::string type = layer["__type"].get<std::string>();

        if (type == "Entities") {
            const auto &grid_pos = layer["entityInstances"][0]["__grid"];
            player->pos.x = grid_pos[0].get<int>() * TILESIZE;
            player->pos.y = grid_pos[1].get<int>() * TILESIZE;
        } else if (type == "Tiles") {
            level->tileset =
                "ressources/" + layer["__tilesetRelPath"].get<std::string>();

            Image image = LoadImage(level->tileset.c_str());
            if (image.data == nullptr) {
                std::cerr << "Failed to load tileset: " << level->tileset
                          << "\n";
                return 1;
            }
            *texture = LoadTextureFromImage(image);
            UnloadImage(image);

            if (texture->id == 0) {
                std::cerr << "Failed to create texture\n";
                return 1;
            }

            const auto &grid_tiles = layer["gridTiles"];
            level->tiles.resize(grid_tiles.size());

            int i = 0;
            for (const auto &tile : grid_tiles) {
                level->tiles[i].world_pos = {tile["px"][0], tile["px"][1]};
                level->tiles[i].src_pos = {tile["src"][0], tile["src"][1]};
                i++;
            }
        } else if (type == "IntGrid") {
            // Build collision grid
            int width = layer["__cWid"];
            int height = layer["__cHei"];
            level->collision_width = width;
            level->collision_height = height;

            level->collision_grid.resize(width * height);

            for (int i = 0; i < width * height; i++) {
                level->collision_grid[i] = layer["intGridCsv"][i];
            }
        }
    }
    return 0;
}

// Helper functions
TileBounds GetTileBounds(Vector2 pos) {
    // Returns surrounding tiles in gridspace
    return {(int)floor(pos.x / TILESIZE),
            (int)floor((pos.x + TILESIZE - 0.01f) / TILESIZE),
            (int)floor(pos.y / TILESIZE),
            (int)floor((pos.y + TILESIZE - 0.01f) / TILESIZE)};
}

int main() {
    int DEBUG = 1;

    InitWindow(screenWidth, screenHeight, "Sidescroller");
    SetTargetFPS(60);

    Player player;
    if (SetupPlayer(&player) != 0) {
        CloseWindow();
        return 1;
    }

    Level level;
    Texture2D texture;
    if (LoadLevel(&level, &texture, &player) != 0) {
        CloseWindow();
        return 1;
    }

    // Setup camera and define important constants
    Camera2D cam = {.offset = {screenWidth / 2.0f, screenHeight / 2.0f},
                    .target = player.pos,
                    .rotation = 0.0,
                    .zoom = 5.0f};
    const float half_view_x = screenWidth / (2.0f * cam.zoom);
    const float half_view_y = screenHeight / (2.0f * cam.zoom);

    const float level_left = level.world_pos.x;
    const float level_right =
        level.world_pos.x + level.collision_width * TILESIZE;
    const float level_top = level.world_pos.y;
    const float level_bottom =
        level.world_pos.y + level.collision_height * TILESIZE;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        // Horizontal movement and collision
        float dx = 0.0f;
        if (IsKeyDown(KEY_A)) {
            dx = -player.walk_speed * dt;
        } else if (IsKeyDown(KEY_D)) {
            dx = player.walk_speed * dt;
        }

        player.touches_wall = false;

        if (dx != 0) {
            player.pos.x += dx;
            TileBounds bounds = GetTileBounds(player.pos);

            for (int y = bounds.top; y <= bounds.bottom; ++y) {
                for (int x = bounds.left; x <= bounds.right; ++x) {
                    // Check grid bounds
                    if (x < 0 || x >= level.collision_width || y < 0 ||
                        y >= level.collision_height) {
                        continue;
                    }
                    if (level.collision_grid[y * level.collision_width + x] ==
                        1) {
                        player.touches_wall = true;
                        if (dx > 0) {
                            player.pos.x = x * TILESIZE - TILESIZE;
                        } else if (dx < 0) {
                            player.pos.x = (x + 1) * TILESIZE;
                        }
                    }
                }
            }
        }

        if (player.touches_wall) {
            player.wall_coyote_timer = player.wall_coyote_time;
        } else {
            if (player.wall_coyote_timer > 0.0f) {
                player.wall_coyote_timer -= dt;
            }
        }

        // Vertical movement and collision
        if (player.vel.y >= 0) {
            player.vel.y += player.fall_gravity * dt;
        } else {
            player.vel.y += player.jump_gravity * dt;
        }

        if (IsKeyPressed(KEY_SPACE)) {
            if (player.grounded) {
                player.vel.y = -player.jump_velocity;
                player.grounded = false;
            } else if ((player.touches_wall ||
                        player.wall_coyote_timer > 0.0f)) {
                player.vel.y = -player.jump_velocity;
            }
        }

        float dy = player.vel.y * dt;
        if (dy != 0) {
            player.pos.y += dy;
            TileBounds bounds = GetTileBounds(player.pos);

            for (int y = bounds.top; y <= bounds.bottom; ++y) {
                for (int x = bounds.left; x <= bounds.right; ++x) {
                    // Check grid bounds
                    if (x < 0 || x >= level.collision_width || y < 0 ||
                        y >= level.collision_height) {
                        continue;
                    }
                    if (level.collision_grid[y * level.collision_width + x] ==
                        1) {
                        if (dy > 0) {
                            player.pos.y = y * TILESIZE - TILESIZE;
                            player.grounded = true;
                        } else if (dy < 0) {
                            player.pos.y = (y + 1) * TILESIZE;
                        }
                        player.vel.y = 0.0f;
                    }
                }
            }
        }

        // Move and Clamp Camera
        cam.target.x =
            Clamp(player.pos.x + TILESIZE / 2.0f, level_left + half_view_x,
                  level_right - half_view_x);
        cam.target.y =
            Clamp(player.pos.y + TILESIZE / 2.0f, level_top + half_view_y,
                  level_bottom - half_view_y);

        BeginDrawing();

        ClearBackground({40, 40, 46});

        BeginMode2D(cam);
        // Draw Map
        for (int i = 0; i < level.tiles.size(); ++i) {
            Tile tile = level.tiles[i];
            Rectangle src = {.x = tile.src_pos.x,
                             .y = tile.src_pos.y,
                             .width = TILESIZE,
                             .height = TILESIZE};
            Rectangle dest = {.x = tile.world_pos.x,
                              .y = tile.world_pos.y,
                              .width = TILESIZE,
                              .height = TILESIZE};
            DrawTexturePro(texture, src, dest, level.world_pos, 0.0f, WHITE);
        }

        // Draw Player
        Rectangle src = {.x = 0, .y = 0, .width = TILESIZE, .height = TILESIZE};
        Rectangle dest = {.x = player.pos.x,
                          .y = player.pos.y,
                          .width = TILESIZE,
                          .height = TILESIZE};
        DrawTexturePro(texture, src, dest, level.world_pos, 0.0f, WHITE);
        EndMode2D();

        if (DEBUG) {
            DrawText(TextFormat("X: %d | Y: %d",
                                (int)floor(player.pos.x / TILESIZE),
                                (int)floor(player.pos.y / TILESIZE)),
                     0, 0, 20, WHITE);
        }
        EndDrawing();
    }
    CloseWindow();

    // Cleanup
    level.tiles.clear();
    level.collision_grid.clear();
    UnloadTexture(texture);

    return 0;
}
