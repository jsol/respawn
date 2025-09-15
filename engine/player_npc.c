#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "map.h"
#include "map_opts.h"
#include "message.h"
#include "player.h"
#include "player_npc.h"
#include "portals.h"
#include "spell.h"

#define ACCEPTABLE_LOS 45

struct ctx {
  char tag[4];
  message_t *to_server;
  map_t *map;
  portals_ctx_t *portals;
  player_t *me;
  player_t *players;
  uint8_t player_count;
  map_opts_t *poi;
};

void *player_npc_new(void) {
  struct ctx *c;

  c = malloc(sizeof(*c));

  strcpy(c->tag, "PNC");

  c->to_server = NULL;
  c->poi = map_opts_new(20);

  return c;
}

void player_npc_free(void **data) {
  struct ctx *c = (struct ctx *)(*data);

  if (data == NULL || strcmp(c->tag, "PNC") != 0) {
    return;
  }

  message_unref(c->to_server);

  map_opts_free(c->poi);
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

static pos_t select_move(struct ctx *ctx, pos_t *opts, uint32_t opts_num) {
  pos_t pos = POSITION_UNKNOWN;
  uint8_t spell_opts = 0;

  map_opts_t *inc = NULL;

   printf("======= DETERMINING MOVE ===========\n");
 /*
  printf("Starting position: (%d,%d)\n", ctx->me->position.x,
         ctx->me->position.y);
  printf("Options: ");
*/
  if (opts_num == 0) {
    printf("No options provided! \n");
    return ctx->me->position;
  }
  inc = map_opts_import(opts, opts_num);

  for (uint8_t i = 0; i < PORTAL_NONE; i++) {
    if (ctx->me->spells[i] == NULL || ctx->me->charges[i] <= 0 ||
        ctx->me->spells[i]->defencive == true) {
      continue;
    }
    spell_opts++;
  }

  if (spell_opts < 2) {
    printf("Not enough spells, hunt an active portal\n");
    map_opts_t *portals = map_opts_new(portals_num(ctx->portals));

    for (uint8_t i = 0; i < portals_num(ctx->portals); i++) {
      portal_t *p = portals_get(ctx->portals, i);
      if (ctx->me->spells[p->kind] == NULL &&
          map_within_distance(ctx->map, ctx->me->position, p->position, 31)) {
        map_opts_add(portals, p->position);
      }
    }


    pos_t target = map_closest(ctx->map, ctx->me->position, portals);
    if (!POS_IS_UNKNOWN(target)) {
      printf("Found the closest portal: (%d,%d)\n", target.x, target.y);
    } else {
      printf("All portals too far away or not interesting\n");
    }
    pos = map_closest(ctx->map, target, inc);

    map_opts_free(portals);

    if (!POS_IS_UNKNOWN(pos)) {
      printf("Found the closest option in opts: (%d,%d)\n", pos.x, pos.y);

      goto out;
    }
  }

  if (ctx->poi->size == 0) {
    printf("No points of interest, picking at random\n");
    pos = opts[rand() % opts_num];
    goto out;
  }

  for (uint32_t i = 0; i < opts_num; i++) {
    if (map_opts_contains(ctx->poi, opts[i])) {
      pos = opts[i];
      printf("Points of interest, within reach, go there\n");
      goto out;
    }
  }

  pos_t target = map_closest(ctx->map, ctx->me->position, ctx->poi);
  if (!POS_IS_UNKNOWN(pos)) {
    printf("Found the PoI: (%d,%d)\n", target.x, target.y);
  }
  pos = map_closest(ctx->map, target, inc);

  if (POS_IS_UNKNOWN(pos)) {
    printf("Points of interest too far, picking at random\n");
    pos = opts[rand() % opts_num];
  }

out:
  map_opts_free(inc);
  printf("Move to (%d,%d)\n", pos.x, pos.y);
  printf(" ========= DONE SELECTING MOVE ===========\n");
  return pos;
}

static uint8_t select_direction(struct ctx *ctx, pos_t pos) {
  uint8_t opts[DIRECTION_ANY];
  uint32_t max;
  uint8_t candidate;

  /* Pick out a direction resulting in a suitably big LOS if possible */

  for (uint8_t i = 0; i < DIRECTION_ANY; i++) {
    opts[i] = i;
  }

  for (uint8_t i = 0; i < DIRECTION_ANY; i++) {
    uint8_t from, to, tmp;

    from = rand() % DIRECTION_ANY;
    to = rand() % DIRECTION_ANY;

    tmp = opts[to];
    opts[to] = opts[from];
    opts[from] = tmp;
  }

  for (uint8_t i = 0; i < DIRECTION_ANY; i++) {
    map_opts_t *los = map_line_of_sight(ctx->map, pos, opts[i]);
    if (los->size > ACCEPTABLE_LOS) {
      map_opts_free(los);
      return opts[i];
    }
    if (los->size > max) {
      max = los->size;
      candidate = opts[i];
    }
    map_opts_free(los);
  }

  return candidate;
}

static int32_t score_spell(struct ctx *ctx, player_t *target,
                           const spell_t *spell) {
  int32_t score = 0;
  int8_t hit = 0;
  int8_t dmg_max = 0;
  int8_t dmg_min = 0;
  coord_t dist;

  if (spell == NULL || ctx->me->charges[spell->kind] == 0) {
    return 0;
  }

  dist = map_distance_squared(ctx->map, ctx->me->position, target->position);

  spell_get_stats(spell, dist, &hit, &dmg_min, &dmg_max);

  score = hit * (dmg_min + dmg_max);

  score += spell->num_effects * 5;
  score += (100 - target->health) * 5;

  return score;
}

static void select_fight(struct ctx *ctx, message_t *msg) {
  int32_t scores[ctx->player_count][PORTAL_NONE];
  player_t *target = NULL;
  const spell_t *spell = NULL;
  int32_t max = 0;

  for (uint8_t i = 0; i < ctx->player_count; i++) {
    player_t *p = &ctx->players[i];
    if (p == ctx->me || POS_IS_UNKNOWN(p->position)) {
      scores[i][0] = 0;
      scores[i][1] = 0;
      scores[i][2] = 0;
      scores[i][3] = 0;

      continue;
    }

    for (uint8_t j = 0; j < PORTAL_NONE; j++) {
      scores[i][j] = score_spell(ctx, &ctx->players[i], ctx->me->spells[j]);
    }
  }

  for (uint8_t i = 0; i < ctx->player_count; i++) {

    for (uint8_t j = 0; j < PORTAL_NONE; j++) {
      if (scores[i][j] > max) {
        target = &ctx->players[i];
        spell = ctx->me->spells[j];
      }
    }
  }

  if (spell != NULL && target != NULL) {
    reply(ctx, message_reply_fight(msg->tick, spell->id, target->position));
  }
}

void player_npc_server_send(void *data, message_t *msg) {
  struct ctx *ctx = (struct ctx *)(data);
  pos_t tmp;
  uint8_t face;

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
    reply(ctx, message_reply_spawn(
                   msg->tick, msg->body.ask_spawn.opts[0],
                   select_direction(ctx, msg->body.ask_spawn.opts[0])));

    break;

  case MESSAGE_ASK_MOVE:
    tmp = select_move(ctx, msg->body.ask_move.opts, msg->body.ask_move.size);
    face = select_direction(ctx, tmp);
    reply(ctx, message_reply_move(msg->tick, tmp, face));

    break;

  case MESSAGE_ASK_FIGHT: {

    select_fight(ctx, msg);
    if (ctx->to_server == NULL && ctx->me->health < 100) {
      for (uint8_t j = 0; j < PORTAL_NONE; j++) {
        if (ctx->me->charges[j] > 0 && ctx->me->spells[j] != NULL &&
            ctx->me->spells[j]->defencive) {
          reply(ctx, message_reply_fight(msg->tick, ctx->me->spells[j]->id,
                                         ctx->me->position));
        }
      }
    }

    if (ctx->to_server == NULL) {
      reply(ctx, message_reply_fight(msg->tick, 0, POSITION_UNKNOWN));
    }
    break;
  }

  case MESSAGE_PLAYER_UPDATE:
    player_batch_update(ctx->players, ctx->player_count, msg);
    portals_update(ctx->portals, msg);

    for (uint8_t i = 0; i < portals_num(ctx->portals); i++) {
      portal_t *p = portals_get(ctx->portals, i);
      if (p->spell != NULL && ctx->me->spells[p->spell->kind] == NULL) {
        map_opts_add(ctx->poi, p->position);
      }
    }

    for (uint8_t i = 0; i < msg->body.player_update.num_events; i++) {
      map_opts_t *add;

      add =
          map_valid_moves(ctx->map, msg->body.player_update.events[i].from, 3);

      map_opts_shuffle(add);

      map_opts_add(ctx->poi, add->data[0]);
      map_opts_free(add);
    }

    map_opts_delete(ctx->poi, ctx->me->position);

    reply(ctx, message_reply_player_update(msg->tick));

  default:
    break;
  }
}
