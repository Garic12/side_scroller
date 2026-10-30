#include "player.h"

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

    player->wall_cooldown_time = 0.5f;
    player->wall_coyote_timer = 0.0f;

    player->walk_speed = 45.0f;

    player->pos = level->player_start;
    player->last_entry = level->player_start;

    player->has_key = false;

    return 0;
}

Rectangle GetPlayerHitbox(const Player &player) {
    return {player.pos.x + player.body.offset.x,
            player.pos.y + player.body.offset.y, player.body.size.x,
            player.body.size.y};
}

Rectangle GetPlayerHurtbox(const Player &player) {
    return {player.pos.x + player.hurtbox.offset.x,
            player.pos.y + player.hurtbox.offset.y, player.hurtbox.size.x,
            player.hurtbox.size.y};
}
