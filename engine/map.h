#pragma once
#include <stdbool.h>
#include <stdint.h>

#include "common.h"
#include "map_opts.h"
#include "message.h"

typedef struct map_ctx map_t;

enum player { PLAYER_1 = (1 << 3), PLAYER_2 = (1 << 4) };

map_t *map_new(coord_t width, coord_t height, int32_t room_factor,
               uint32_t seed);
map_t *map_new_from_message(message_t *msg);

coord_t map_height(map_t *ctx);
coord_t map_width(map_t *ctx);

bool map_is_wall(map_t *ctx, pos_t pos);
bool map_is_player(map_t *ctx, pos_t pos);
bool map_is_portal(map_t *ctx, pos_t pos);
void map_set_player(map_t *ctx, pos_t pos);
void map_unset_player(map_t *ctx, pos_t pos);
void map_set_portal(map_t *ctx, pos_t pos);
void map_unset_portal(map_t *ctx, pos_t pos);
map_opts_t *map_valid_spawns(map_t *ctx, uint32_t num, uint8_t safe_zone);
map_opts_t *map_valid_moves(map_t *ctx, pos_t pos, uint8_t steps);
map_opts_t *map_empty_spaces(map_t *ctx);
map_opts_t *map_line_of_sight(map_t *ctx, pos_t pos, enum direction dir);
bool map_has_los(map_t *ctx, pos_t from, pos_t to);
pos_t map_ends_up_at(map_t *ctx, pos_t from, pos_t to);
pos_t map_push(map_t *ctx, pos_t from, pos_t to, coord_t steps);
pos_t map_pull(map_t *ctx, pos_t from, pos_t to, coord_t steps);
pos_t map_closest(map_t *ctx, pos_t from, map_opts_t *opts);

map_opts_t *map_players(map_t *ctx, map_opts_t *at);
map_opts_t *map_portals(map_t *ctx, map_opts_t *at);

bool map_within_distance(map_t *ctx, pos_t from, pos_t to, coord_t range);

coord_t map_distance_squared(map_t *ctx, pos_t from, pos_t to);

map_opts_t *map_reduce_to_distance(map_t *ctx, pos_t pos, map_opts_t *opts,
                                   coord_t dist);

bool map_valid_direction(enum direction dir);

message_t *map_to_message(map_t *map, uint32_t tick);
