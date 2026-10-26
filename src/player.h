#pragma once
#include <level.h>
#include <raylib.h>

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

int SetupPlayer(Player *player, Level *level);
