#pragma once

#include "message.h"

void* player_local_new(void);

void player_local_free(void** ctx);

message_t * player_local_get(void *ctx);
void player_local_send(void *ctx, message_t *msg);

message_t * player_local_server_get(void *ctx);
void player_local_server_send(void *ctx, message_t *msg);
