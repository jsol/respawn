#pragma once

#include "message.h"

void* player_npc_new(void);
void player_npc_free(void** ctx);

message_t * player_npc_server_get(void *ctx);
void player_npc_server_send(void *ctx, message_t *msg);
