#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "map.h"
#include "message.h"
#include "player.h"
#include "player_npc.h"
#include "portals.h"

struct ctx {
  char tag[4];
  message_t *to_server;
  map_t *map;
  portals_ctx_t *portals;
  player_t *me;
  player_t *players;
  uint8_t player_count;
};

void *player_npc_new(void) {
  struct ctx *c;

  c = malloc(sizeof(*c));

  strcpy(c->tag, "PNC");

  c->to_server = NULL;

  return c;
}

void player_npc_free(void **data) {
  struct ctx *c = (struct ctx *)(*data);

  if (data == NULL || strcmp(c->tag, "PNC") != 0) {
    return;
  }

  message_unref(c->to_server);

  free(c);
  *data = NULL;
}

message_t *player_npc_server_get(void *ctx) {
  message_t *tmp;
  struct ctx *c = (struct ctx *)(ctx);

  if (c == NULL) {
    return NULL;
  }

  tmp = c->to_server;

  c->to_server = NULL;

  return tmp;
}

static void reply(struct ctx *ctx, message_t *msg) {

  if (ctx->to_server != NULL) {
    printf("Warning: NPC -> server message not collected\n");
    message_unref(ctx->to_server);
  }
  ctx->to_server = msg;
}

void player_npc_server_send(void *data, message_t *msg) {
  struct ctx *ctx = (struct ctx *)(data);

  if (ctx == NULL) {
    return;
  }

  switch (msg->type) {
  case MESSAGE_ASK_READY:
    reply(ctx, message_reply_ready(msg->tick));
    break;

  case MESSAGE_MAP:
    ctx->map = map_new_from_message(msg);
    ctx->portals = portals_new_from_message(msg);
    ctx->player_count = msg->body.map.num_players;
    ctx->players = player_create(ctx->player_count);

    ctx->to_server = message_reply_map(msg->tick);
    break;

  case MESSAGE_ASK_SPAWN:
    ctx->me = &ctx->players[msg->body.ask_spawn.player_id];
    reply(ctx, message_reply_spawn(msg->tick, msg->body.ask_spawn.opts[0],
                                   rand() % DIRECTION_ANY));

    break;

  case MESSAGE_ASK_MOVE:
    reply(ctx, message_reply_move(
        msg->tick, msg->body.ask_move.opts[rand() % msg->body.ask_move.size],
        rand() % DIRECTION_ANY));

    break;

  case MESSAGE_ASK_FIGHT:
    for (uint8_t i = 0; i < ctx->player_count; i++) {
      player_t *p = &ctx->players[i];
      uint8_t spell_id = 0;
      if (p == ctx->me || POS_IS_UNKNOWN(p->position)) {
        continue;
      }

      for (uint8_t j = 0; j < PORTAL_NONE; j++) {
        if (p->charges[j] > 0) {
          spell_id = p->spells[j]->id;
        }
      }

      if (spell_id == 0) {
        continue;
      }

      reply(ctx, message_reply_fight(msg->tick, spell_id, p->position));
      break;
    }

    if (ctx->to_server == NULL) {
      reply(ctx, message_reply_fight(msg->tick, 0, POSITION_UNKNOWN));
    }
    break;

  case MESSAGE_PLAYER_UPDATE:
    player_batch_update(ctx->players, ctx->player_count, msg);

    reply(ctx, message_reply_player_update(msg->tick));

  default:
    break;
  }
}
