#pragma once
#include <level.h>
#include <raylib.h>

struct Hitbox {
        Vector2 offset;
        Vector2 size;
};

struct Player {
        float jump_height;
        float jump_time_to_peak;
        float jump_time_to_fall;

        float jump_velocity;
        float jump_gravity;
        float fall_gravity;

        float walk_speed;

        Vector2 pos = {0.0f, 0.0f};
        Vector2 last_entry = {0.0f, 0.0f};
        Vector2 vel = {0.0f, 0.0f};

        Hitbox body = {{0.0f, 0.0f}, {8.0f, 8.0f}};
        Hitbox hurtbox = {{1.0f, 2.0f}, {5.0f, 4.0f}};
        bool grounded = true;
        bool touches_wall;

        float wall_coyote_timer;
        float wall_coyote_time;

        float wall_cooldown_timer;
        float wall_cooldown_time;
};

int SetupPlayer(Player *player, Level *level);
Rectangle GetPlayerHitbox(const Player &player);
Rectangle GetPlayerHurtbox(const Player &player);
