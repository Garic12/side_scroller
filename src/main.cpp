#include "nlohmann/detail/value_t.hpp"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <raylib.h>
#include <raymath.h>
#include <vector>

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

struct TileBounds {
        int left, right, top, bottom;
};

struct Tile {
        Vector2 world_pos;
        Vector2 src_pos;
};

struct Door {
        Vector2 pos;

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

struct Input {
        float move_x;
        bool jump_pressed;
};

// Game logic related functions
int SetupPlayer(Player *player, Level *level) {
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

    player->pos = {level->player_start.x * TILESIZE,
                   level->player_start.y * TILESIZE};
    std::cout << level->player_start.x << "|" << level->player_start.y
              << std::endl;

    return 0;
}

Level LoadLevel(const json &data) {
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
                    const float x = entity["__grid"][0].get<float>();
                    const float y = entity["__grid"][1].get<float>();
                    level.player_start = {x, y};
                }
            }
        } else if (type == "Tiles") {
            const auto &grid_tiles = layer["gridTiles"];

            level.tiles.reserve(grid_tiles.size());

            for (const auto &tile : grid_tiles) {
                level.tiles.push_back(
                    {.world_pos = {tile["px"][0], tile["px"][1]},
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

Input GatherInput() {
    Input input = {};
    if (IsKeyDown(KEY_A))
        input.move_x -= 1.0f;
    if (IsKeyDown(KEY_D))
        input.move_x += 1.0f;
    input.jump_pressed = IsKeyPressed(KEY_SPACE);
    return input;
}

// Helper functions
TileBounds GetTileBounds(Vector2 pos) {
    // Returns surrounding tiles in gridspace
    return {(int)floor(pos.x / TILESIZE),
            (int)floor((pos.x + TILESIZE - 0.01f) / TILESIZE),
            (int)floor(pos.y / TILESIZE),
            (int)floor((pos.y + TILESIZE - 0.01f) / TILESIZE)};
}

bool IsSolid(const Level *level, int x, int y) {
    if (x < 0 || x >= level->collision_width || y < 0 ||
        y >= level->collision_height)
        return false;
    return level->collision_grid[y * level->collision_width + x] == 1;
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
