#include "globals.h"
#include "level.h"
#include "player.h"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <raylib.h>
#include <raymath.h>
#include <vector>

using json = nlohmann::json;

struct Input {
        float move_x;
        bool jump_pressed;
};

Input GatherInput() {
    Input input = {};
    if (IsKeyDown(KEY_A))
        input.move_x -= 1.0f;
    if (IsKeyDown(KEY_D))
        input.move_x += 1.0f;
    input.jump_pressed = IsKeyPressed(KEY_SPACE);
    return input;
}

int main() {
    int DEBUG = 1;

    InitWindow(screenWidth, screenHeight, "Sidescroller");
    SetTargetFPS(60);

    // Load world with all sublevels
    // Read json
    std::ifstream file("ressources/main.ldtk");
    if (!file) {
        std::cerr << "Could not open main.ldtk\n";
        return 1;
    }

    // Load texture
    Image image = LoadImage("ressources/map/tileset.png");
    if (image.data == nullptr) {
        std::cerr << "Failed to load tileset: " << "\n";
        return 1;
    }
    Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    if (texture.id == 0) {
        std::cerr << "Failed to create texture\n";
        return 1;
    }

    // Go down to level part
    json ldtk = json::parse(file);
    const auto level_arr = ldtk["levels"];

    World world;
    world.levels.reserve(level_arr.size());
    for (const auto &level : level_arr) {
        world.levels.push_back(LoadLevel(level));
    }
    world.current_level = &world.levels[0];
    Level *level = world.current_level;

    Player player;
    if (SetupPlayer(&player, world.current_level) != 0) {
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

    const float level_left = level->world_pos.x;
    const float level_right =
        level->world_pos.x + level->collision_width * TILESIZE;
    const float level_top = level->world_pos.y;
    const float level_bottom =
        level->world_pos.y + level->collision_height * TILESIZE;

    // --- Game Loop ---
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        Input input = GatherInput();

        // Horizontal movement and collision
        float dx = input.move_x * player.walk_speed * dt;
        player.touches_wall = false;
        if (dx != 0) {
            player.pos.x += dx;
            TileBounds bounds = GetTileBounds(player.pos);
            for (int y = bounds.top; y <= bounds.bottom; ++y) {
                for (int x = bounds.left; x <= bounds.right; ++x) {
                    // Check grid bounds
                    if (IsSolid(level, x, y)) {
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

        // Walljump Coyote
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

        if (input.jump_pressed) {
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
                    if (IsSolid(level, x, y)) {
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
        for (int i = 0; i < level->tiles.size(); ++i) {
            Tile tile = level->tiles[i];
            Rectangle src = {.x = tile.src_pos.x,
                             .y = tile.src_pos.y,
                             .width = TILESIZE,
                             .height = TILESIZE};
            Rectangle dest = {.x = tile.world_pos.x,
                              .y = tile.world_pos.y,
                              .width = TILESIZE,
                              .height = TILESIZE};
            DrawTexturePro(texture, src, dest, level->world_pos, 0.0f, WHITE);
        }

        // Draw Player
        Rectangle src = {.x = 0, .y = 0, .width = TILESIZE, .height = TILESIZE};
        Rectangle dest = {.x = player.pos.x,
                          .y = player.pos.y,
                          .width = TILESIZE,
                          .height = TILESIZE};
        DrawTexturePro(texture, src, dest, level->world_pos, 0.0f, WHITE);
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
    level->tiles.clear();
    level->collision_grid.clear();
    UnloadTexture(texture);

    return 0;
}
