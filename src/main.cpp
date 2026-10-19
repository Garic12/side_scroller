#include <algorithm>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <raylib.h>
#include <raymath.h>

using json = nlohmann::json;

struct Player {
        Vector2 pos;
        Vector2 vel;
        float speed;
        bool grounded;
};

struct Tile {
        Vector2 world_pos;
        Vector2 src_pos;
};

struct Level {
        Vector2 world_pos;
        Vector2 world_size;
        int num_tiles;
        Tile *tiles;
        std::vector<int> collision_grid;
        int collision_width;
        int collision_height;
        std::string tileset;
};

// Global values
int screenWidth = 800;
int screenHeight = 450;

float TILESIZE = 32.0f;
float GRAVITY = 270;
float JUMP_SPEED = 100;

int main() {
    InitWindow(screenWidth, screenHeight, "Sidescroller");
    SetTargetFPS(30);

    Player player;
    player.pos = {0, 0};
    player.speed = 140;
    player.grounded = true;

    // Parse map information
    std::ifstream i("ressources/main.ldtk");
    json ldtk = json::parse(i);

    // Feed the map information into our own structures
    Level level;
    Texture2D texture;

    const auto map = ldtk["levels"][0];
    level.world_pos = {map["worldX"], map["worldY"]};
    level.world_size = {map["pxWid"], map["pxHei"]};

    for (const auto &layer : map["layerInstances"]) {
        if (layer["__type"] == "Entities") {
            player.pos.x =
                layer["entityInstances"][0]["__grid"][0].get<int>() * TILESIZE;
            player.pos.y =
                layer["entityInstances"][0]["__grid"][1].get<int>() * TILESIZE;
        } else if (layer["__type"] == "IntGrid") {
            // Scan in tile information
            level.tileset =
                "ressources/" + layer["__tilesetRelPath"].get<std::string>();
            Image image = LoadImage(level.tileset.c_str());
            texture = LoadTextureFromImage(image);

            int num_tiles = layer["autoLayerTiles"].size();
            level.num_tiles = num_tiles;
            level.tiles = new Tile[num_tiles];

            int i = 0;
            for (const auto &tile : layer["autoLayerTiles"]) {
                level.tiles[i].world_pos = {tile["px"][0], tile["px"][1]};
                level.tiles[i].src_pos = {tile["src"][0], tile["src"][1]};
                i++;
            }

            // Build collision grid
            int width = layer["__cWid"];
            int height = layer["__cHei"];
            level.collision_width = layer["__cWid"];
            level.collision_height = layer["__cHei"];

            level.collision_grid.resize(width * height);

            for (int i = 0; i < width * height; i++) {
                level.collision_grid[i] = layer["intGridCsv"][i];
            }
        }
    }

    Camera2D cam = {.offset = {screenWidth / 2.0f, screenHeight / 2.0f},
                    .target = player.pos,
                    .rotation = 0.0,
                    .zoom = 1.5f};
    float half_view_x = screenWidth / (2.0f * cam.zoom);
    float half_view_y = screenHeight / (2.0f * cam.zoom);

    float level_left = level.world_pos.x;
    float level_right = level.world_pos.x + level.collision_width * TILESIZE;
    float level_top = level.world_pos.y;
    float level_bottom = level.world_pos.y + level.collision_height * TILESIZE;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        float dx = 0.0f;
        if (IsKeyDown(KEY_A)) {
            dx = -player.speed * dt;
        } else if (IsKeyDown(KEY_D)) {
            dx = player.speed * dt;
        }

        // Horizontal movement and collision
        if (dx != 0) {
            player.pos.x += dx;
            int left = (int)floor(player.pos.x / TILESIZE);
            int right =
                (int)floor((player.pos.x + TILESIZE - 0.01f) / TILESIZE);
            int top = (int)floor(player.pos.y / TILESIZE);
            int bottom =
                (int)floor((player.pos.y + TILESIZE - 0.01f) / TILESIZE);

            for (int y = top; y <= bottom; ++y) {
                for (int x = left; x <= right; ++x) {
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

        player.vel.y += GRAVITY * dt;
        if (IsKeyPressed(KEY_SPACE)) {
            player.vel.y = -JUMP_SPEED;
            player.grounded = false;
        }

        float dy = player.vel.y * dt;
        // Vertical movement and collision
        if (dy != 0) {
            player.pos.y += dy;
            int left = (int)floor(player.pos.x / TILESIZE);
            int right =
                (int)floor((player.pos.x + TILESIZE - 0.01f) / TILESIZE);
            int top = (int)floor(player.pos.y / TILESIZE);
            int bottom =
                (int)floor((player.pos.y + TILESIZE - 0.01f) / TILESIZE);

            for (int y = top; y <= bottom; ++y) {
                for (int x = left; x <= right; ++x) {
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

        // Move Camera

        cam.target.x =
            Clamp(player.pos.x + TILESIZE / 2.0f, level_left + half_view_x,
                  level_right - half_view_x);
        cam.target.y =
            Clamp(player.pos.y + TILESIZE / 2.0f, level_top + half_view_y,
                  level_bottom - half_view_y);

        BeginDrawing();
        ClearBackground(BLUE);
        BeginMode2D(cam);
        // Draw Map
        for (int i = 0; i < level.num_tiles; ++i) {
            Tile tile = level.tiles[i];
            // std::cout << "Tile " << i << ": px:" << tile.src_pos.x << ", "
            //           << tile.src_pos.y << std::endl;
            Rectangle src = {.x = tile.src_pos.x,
                             .y = tile.src_pos.y,
                             .width = 32,
                             .height = 32};
            Rectangle dest = {.x = tile.world_pos.x,
                              .y = tile.world_pos.y,
                              .width = 32,
                              .height = 32};
            DrawTexturePro(texture, src, dest, level.world_pos, 0.0f, WHITE);
        }

        // Draw Player
        DrawRectangleRec(Rectangle{player.pos.x, player.pos.y, 32.0f, 32.0f},
                         BLACK);
        EndMode2D();
        DrawText(TextFormat("X: %d | Y: %d",
                            (int)floor(player.pos.x / TILESIZE),
                            (int)floor(player.pos.y / TILESIZE)),
                 0, 0, 20, WHITE);
        EndDrawing();
    }
    CloseWindow();

    delete level.tiles;

    // Cleanup collision grid
    level.collision_grid.clear();

    UnloadTexture(texture);
    return 1;
}
