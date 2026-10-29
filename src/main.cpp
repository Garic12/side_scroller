#include "globals.h"
#include "level.h"
#include "player.h"
#include <fstream>
#include <imgui.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include <raylib.h>
#include <raymath.h>
#include <rlImGui.h>
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

Vector2 Perpendicular(Vector2 v) { return {-v.y, v.x}; }

Vector2 ProjectionMinMax(const Vector2 axis, const Vector2 *points, int size) {
    float min = FLT_MAX;
    float max = -FLT_MAX;

    for (int i = 0; i < size; i++) {
        float projection = Vector2DotProduct(points[i], axis);

        if (projection < min) {
            min = projection;
        }
        if (projection > max) {
            max = projection;
        }
    }
    return {min, max};
}

bool CollisionRectSpike(const Rectangle &rect, const Spike &spike) {
    // Calculate all axes we want to check the collision over
    const Vector2 axes[]{
        {1.0f, 0.0f},
        {0.0f, 1.0f},
        Perpendicular(Vector2Subtract(spike.a, spike.b)),
        Perpendicular(Vector2Subtract(spike.b, spike.c)),
        Perpendicular(Vector2Subtract(spike.c, spike.a)),
    };

    Vector2 corners_rect[4] = {{rect.x, rect.y},
                               {rect.x + rect.width, rect.y},
                               {rect.x, rect.y + rect.height},
                               {rect.x + rect.width, rect.y + rect.height}};
    Vector2 corners_spike[3] = {spike.a, spike.b, spike.c};

    for (int i = 0; i < 5; i++) {
        Vector2 minmax_rect = ProjectionMinMax(axes[i], corners_rect, 4);
        Vector2 minmax_spike = ProjectionMinMax(axes[i], corners_spike, 3);

        if (minmax_rect.y < minmax_spike.x || minmax_spike.y < minmax_rect.x) {
            return false;
        }
    }
    return true;
}

int main() {
    int DEBUG = 1;

    InitWindow(screenWidth, screenHeight, "Sidescroller");
    SetTargetFPS(60);

    rlImGuiSetup(true);

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

    // --- Game Loop ---
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        Input input = {};
        if (!ImGui::GetIO().WantCaptureKeyboard)
            input = GatherInput();

        // Horizontal movement and collision
        float dx = input.move_x * player.walk_speed * dt;
        player.touches_wall = false;
        if (dx != 0) {
            player.pos.x += dx;

            const Rectangle hitbox = GetPlayerHitbox(player);
            const TileBounds bounds = GetTileBounds(hitbox);

            for (int y = bounds.top; y <= bounds.bottom; ++y) {
                for (int x = bounds.left; x <= bounds.right; ++x) {
                    // Check grid bounds
                    const int collision_id =
                        IsColliding(world.current_level, x, y);
                    if (collision_id == 1) {
                        player.touches_wall = true;

                        if (dx > 0.0f) {
                            player.pos.x = x * TILESIZE - player.body.offset.x -
                                           player.body.size.x;
                        } else if (dx < 0.0f) {
                            player.pos.x =
                                (x + 1) * TILESIZE - player.body.offset.x;
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
                if (player.wall_cooldown_timer <= 0.0f) {
                    player.vel.y = -player.jump_velocity;
                    player.wall_cooldown_timer = player.wall_cooldown_time;
                }
            }
        }
        if (player.wall_cooldown_timer > 0.0f) {
            player.wall_cooldown_timer -= dt;
        }

        bool has_gravity = true;
        if (has_gravity) {
            float dy = player.vel.y * dt;
            player.pos.y += dy;
            const Rectangle hitbox = GetPlayerHitbox(player);
            const TileBounds bounds = GetTileBounds(hitbox);

            for (int y = bounds.top; y <= bounds.bottom; ++y) {
                for (int x = bounds.left; x <= bounds.right; ++x) {
                    // Check grid bounds
                    const int collision_id =
                        IsColliding(world.current_level, x, y);
                    if (collision_id == 1) {
                        if (dy > 0.0f) {
                            player.pos.y = y * TILESIZE - player.body.offset.y -
                                           player.body.size.y;
                            player.vel = Vector2Zero();
                            player.grounded = true;
                        } else if (dy < 0.0f) {
                            player.pos.y =
                                (y + 1) * TILESIZE - player.body.offset.y;
                        }
                        player.vel.y = 0.0f;
                    } else if (collision_id == 2) {
                        if (dy > 0.0f) {
                            const float previous_bottom =
                                player.pos.y - dy + TILESIZE;

                            const float tile_top = y * TILESIZE;

                            if (previous_bottom <= tile_top) {
                                player.pos.y = y * TILESIZE -
                                               player.body.offset.y -
                                               player.body.size.y;
                                player.vel = Vector2Zero();
                                player.grounded = true;
                            }
                        }
                    }
                }
            }
        }

        // Check walking through door, maybe do this more efficient someday
        Rectangle player_rect = {player.pos.x, player.pos.y, TILESIZE,
                                 TILESIZE};
        for (const Door &door : world.current_level->doors) {
            if (CheckCollisionRecs(player_rect, door.hitbox)) {
                for (Level &new_level : world.levels) {
                    if (door.target_level == new_level.id) {
                        world.current_level = &new_level;
                        player.pos = door.target_pos;
                        player.last_entry = door.target_pos;
                        break;
                    }
                }
                break;
            }
        }

        // Check spike collision
        bool collision;
        for (const Spike &spike : world.current_level->spikes) {
            if (CollisionRectSpike(GetPlayerHurtbox(player), spike)) {
                player.pos = player.last_entry;
                collision = true;
            } else {
                collision = false;
            }
        }

        // Move and Clamp Camera
        cam.target.x =
            Clamp(player.pos.x + TILESIZE / 2.0f,
                  world.current_level->world_pos.x + half_view_x,
                  world.current_level->world_pos.x +
                      world.current_level->world_size.x - half_view_x);
        cam.target.y =
            Clamp(player.pos.y + TILESIZE / 2.0f,
                  world.current_level->world_pos.y + half_view_y,
                  world.current_level->world_pos.y +
                      world.current_level->world_size.y - half_view_y);

        BeginDrawing();
        ClearBackground({40, 40, 46});

        BeginMode2D(cam);
        // Draw Map
        for (int i = 0; i < world.current_level->tiles.size(); ++i) {
            Tile tile = world.current_level->tiles[i];
            Rectangle src = {.x = tile.src_pos.x,
                             .y = tile.src_pos.y,
                             .width = TILESIZE,
                             .height = TILESIZE};
            Rectangle dest = {.x = tile.world_pos.x,
                              .y = tile.world_pos.y,
                              .width = TILESIZE,
                              .height = TILESIZE};
            DrawTexturePro(texture, src, dest, {0, 0}, 0.0f, WHITE);
        }

        // Draw Player
        Rectangle src = {.x = 0, .y = 0, .width = TILESIZE, .height = TILESIZE};
        Rectangle dest = {.x = player.pos.x,
                          .y = player.pos.y,
                          .width = TILESIZE,
                          .height = TILESIZE};
        DrawTexturePro(texture, src, dest, {0, 0}, 0.0f, WHITE);
        EndMode2D();

        if (DEBUG) {
            rlImGuiBegin();

            ImGui::Begin("Debug");
            ImGui::Text("Frametime: %.3f ms", GetFrameTime() * 1000.0f);
            ImGui::Text("Current Level: %s", (world.current_level->id).c_str());
            ImGui::DragFloat2("Player pos", &player.pos.x);
            ImGui::Text("Walljump Cooldown: %.1f", player.wall_cooldown_timer);
            ImGui::Separator();
            // ImGui::DragFloat("Walk speed", &player.walk_speed, 1.0f);
            ImGui::End();

            // ImGui::ShowDemoWindow(); // shows every available widget,
            // handy as a reference

            rlImGuiEnd();
        }
        EndDrawing();
    }

    rlImGuiShutdown();
    CloseWindow();

    return 0;
}
