#include <nlohmann/json.hpp>
#include <raylib.h>

struct Player {
        Vector2 pos;
        Vector2 vel;
        float speed;
        bool grounded;
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
    player.pos = {5 * TILESIZE, -1 * TILESIZE};
    player.speed = 60;
    player.grounded = true;

    Camera2D cam = {.offset = {screenWidth / 2.0f, screenHeight / 2.0f},
                    .target = player.pos,
                    .rotation = 0.0,
                    .zoom = 2.0};

    while (!WindowShouldClose()) {
        /* This is not needed for now, work on parsing the map json first
        float dt = GetFrameTime();

        float dx;
        if (IsKeyDown(KEY_A)) {
            dx = -player.speed * dt;
        } else if (IsKeyDown(KEY_D)) {
            dx = player.speed * dt;
        }

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

        cam.target = player.pos;
        */

        BeginDrawing();
        ClearBackground(WHITE);

        BeginMode2D(cam);
        DrawRectangleRec(Rectangle{player.pos.x, player.pos.y, 16.0f, 16.0f},
                         BLACK);
        EndMode2D();
        EndDrawing();
    }
    CloseWindow();
    return 1;
}
