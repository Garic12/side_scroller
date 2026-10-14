#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <raylib.h>
#include <typeinfo>

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
        std::string tileset;
};

// Global values
int screenWidth = 800;
int screenHeight = 450;

float TILESIZE = 32.0f;
float GRAVITY = 160;
float JUMP_SPEED = 100;

int main() {
    InitWindow(screenWidth, screenHeight, "Sidescroller");
    SetTargetFPS(30);

    Player player;
    player.pos = {0, 0};
    player.speed = 60;
    player.grounded = true;

    Camera2D cam = {.offset = {screenWidth / 2.0f, screenHeight / 2.0f},
                    .target = player.pos,
                    .rotation = 0.0,
                    .zoom = 2.0f};

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
        }
    }

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        float dx;
        if (IsKeyDown(KEY_A)) {
            dx = -player.speed * dt;
        } else if (IsKeyDown(KEY_D)) {
            dx = player.speed * dt;
        }

        /* This is not needed for now, work on parsing the map json first
        // Horizontal movement and collision
        if (dx != 0) {
            player.pos.x += dx;
        }

        // Vertical movement
        player.vel.y += GRAVITY * dt;
        if (player.grounded && IsKeyPressed(KEY_SPACE)) {
            player.vel.y = -JUMP_SPEED;
        }
        player.pos.y += player.vel.y * dt;

        */
        cam.target = player.pos;

        BeginDrawing();
        ClearBackground(WHITE);

        BeginMode2D(cam);
        // Draw Map
        for (int i = 0; i < level.num_tiles; i++) {
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
        EndDrawing();
    }
    CloseWindow();

    delete level.tiles;

    UnloadTexture(texture);
    return 1;
}
