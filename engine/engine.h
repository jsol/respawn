#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "map.h"
#include "player.h"
#include "portals.h"

typedef struct engine_ctx engine_t;

engine_t *engine_new(uint8_t num_players, map_t *map, portals_ctx_t *portals,
                     bool timer);

bool engine_add_player(engine_t *ctx, player_send_msg_func_t send,
                       void *send_ctx, player_get_msg_func_t get,
                       void *get_ctx);
void engine_tick(engine_t *ctx);
