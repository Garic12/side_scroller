#include <algorithm>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <raylib.h>
#include <raymath.h>

using json = nlohmann::json;

struct Player {
        float gravity = 250;
        float fall_gravity = 325;
        float jump_speed = 80.0f;
        float walk_speed = 35.0f;

        Vector2 pos = {0, 0};
        Vector2 vel = {0, 0};
        bool grounded = true;
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
        int num_tiles = 0;
        std::vector<Tile> tiles = {};
        std::vector<int> collision_grid = {};
        int collision_width = 0;
        int collision_height = 0;
        std::string tileset = "";
};

// Global values
int screenWidth = 800;
int screenHeight = 450;

float TILESIZE = 8.0f;

TileBounds GetTileBounds(Vector2 pos) {
    // Returns surrounding tiles in gridspace
    return {(int)floor(pos.x / TILESIZE),
            (int)floor((pos.x + TILESIZE - 0.01f) / TILESIZE),
            (int)floor(pos.y / TILESIZE),
            (int)floor((pos.y + TILESIZE - 0.01f) / TILESIZE)};
}

int main() {
    InitWindow(screenWidth, screenHeight, "Sidescroller");
    SetTargetFPS(30);

    Player player;
    // Parse map information
    std::ifstream i("ressources/main.ldtk");
    if (!i) {
        std::cerr << "Could not open main.ldtk\n";
        return 1;
    }
    json ldtk = json::parse(i);

    // Feed the map information into our own structures
    Level level;
    Texture2D texture;

    const auto map = ldtk["levels"][0];
    level.world_pos = {map["worldX"], map["worldY"]};
    level.world_size = {map["pxWid"], map["pxHei"]};

    for (const auto &layer : map["layerInstances"]) {
        std::string type = layer["__type"].get<std::string>();
        if (type == "Entities") {
            player.pos.x =
                layer["entityInstances"][0]["__grid"][0].get<int>() * TILESIZE;
            player.pos.y =
                layer["entityInstances"][0]["__grid"][1].get<int>() * TILESIZE;
        } else if (type == "Tiles") {
            // Scan in tile information
            level.tileset =
                "ressources/" + layer["__tilesetRelPath"].get<std::string>();
            Image image = LoadImage(level.tileset.c_str());
            if (image.data == nullptr) {
                std::cerr << "Failed to load tileset: " << level.tileset
                          << "\n";
            }
            texture = LoadTextureFromImage(image);
            UnloadImage(image);

            if (texture.id == 0) {
                std::cerr << "Failed to create texture\n";
                return 1;
            }

            const auto &tiles = layer["gridTiles"];
            int num_tiles = tiles.size();
            level.num_tiles = num_tiles;
            level.tiles.resize(num_tiles);

            int i = 0;
            for (const auto &tile : tiles) {
                level.tiles[i].world_pos = {tile["px"][0], tile["px"][1]};
                level.tiles[i].src_pos = {tile["src"][0], tile["src"][1]};
                i++;
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

    assert(level.tiles.size() > 0);

    // Create camera and camera movement related information
    Camera2D cam = {.offset = {screenWidth / 2.0f, screenHeight / 2.0f},
                    .target = player.pos,
                    .rotation = 0.0,
                    .zoom = 5.0f};
    float half_view_x = screenWidth / (2.0f * cam.zoom);
    float half_view_y = screenHeight / (2.0f * cam.zoom);

    float level_left = level.world_pos.x;
    float level_right = level.world_pos.x + level.collision_width * TILESIZE;
    float level_top = level.world_pos.y;
    float level_bottom = level.world_pos.y + level.collision_height * TILESIZE;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        // Horizontal movement and collision
        float dx = 0.0f;
        if (IsKeyDown(KEY_A)) {
            dx = -player.walk_speed * dt;
        } else if (IsKeyDown(KEY_D)) {
            dx = player.walk_speed * dt;
        }

        if (dx != 0) {
            player.pos.x += dx;
            TileBounds bounds = GetTileBounds(player.pos);

            for (int y = bounds.top; y <= bounds.bottom; ++y) {
                for (int x = bounds.left; x <= bounds.right; ++x) {
                    if (x < 0 || y < 0 || x >= level.collision_width ||
                        y >= level.collision_height ||
                        level.collision_grid[y * level.collision_width + x] !=
                            0) {
                        if (dx > 0) {
                            player.pos.x = x * TILESIZE - TILESIZE;
                        } else if (dx < 0) {
                            player.pos.x = (x + 1) * TILESIZE;
                        }
                    }
                }
            }
        }

        // Vertical movement and collision
        if (player.vel.y >= 0) {
            player.vel.y += player.fall_gravity * dt;
        } else {
            player.vel.y += player.gravity * dt;
        }
        if (IsKeyPressed(KEY_SPACE)) {
            player.vel.y = -player.jump_speed;
            player.grounded = false;
        }

        float dy = player.vel.y * dt;
        if (dy != 0) {
            player.pos.y += dy;
            TileBounds bounds = GetTileBounds(player.pos);

            for (int y = bounds.top; y <= bounds.bottom; ++y) {
                for (int x = bounds.left; x <= bounds.right; ++x) {
                    if (x < 0 || y < 0 || x >= level.collision_width ||
                        y >= level.collision_height ||
                        level.collision_grid[y * level.collision_width + x] !=
                            0) {
                        if (dy > 0) {
                            player.pos.y = y * TILESIZE - TILESIZE;
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
        for (int i = 0; i < level.num_tiles; ++i) {
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
        DrawText(TextFormat("X: %d | Y: %d",
                            (int)floor(player.pos.x / TILESIZE),
                            (int)floor(player.pos.y / TILESIZE)),
                 0, 0, 20, WHITE);
        EndDrawing();
    }
    CloseWindow();

    level.tiles.clear();

    // Cleanup collision grid
    level.collision_grid.clear();

    UnloadTexture(texture);
    return 1;
}
