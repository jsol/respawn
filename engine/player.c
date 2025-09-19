
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "player.h"
#include "spell.h"

static struct player_effect *time_effect(struct player_effect *eff) {
  struct player_effect *next;

  if (eff == NULL) {
    return NULL;
  }

  eff->duration--;

  if (eff->duration > 0) {
    eff->next = time_effect(eff->next);

    return eff;
  }

  next = eff->next;
  free(eff);

  return time_effect(next);
}

static void reset(player_t *ctx) {
  ctx->health = 0;
  ctx->injured_by = 0;
  ctx->tagged = 0;
  ctx->activated_spell = PORTAL_NONE;

  map_opts_free(ctx->los);
  ctx->los = NULL;
  ctx->los = map_opts_new(1);

  for (struct player_effect *eff = ctx->effects; eff != NULL; eff = eff->next) {
    eff->duration = 0;
  }
  ctx->effects = time_effect(ctx->effects);

  for (uint8_t i = 0; i < PORTAL_NONE; i++) {
    ctx->spells[i] = NULL;
  }
}

player_t *player_new(uint32_t id) {
  player_t *ctx;

  ctx = malloc(sizeof(*ctx));

  ctx->facing = DIRECTION_ANY;
  ctx->id = id;
  ctx->kills = 0;
  ctx->deaths = 0;

  reset(ctx);
  return ctx;
}

player_t *player_create(uint32_t num) {
  player_t *players;

  players = malloc(sizeof(*players) * num);

  memset(players, 0, sizeof(*players) * num);

  for (uint32_t i = 0; i < num; i++) {
    players[i].facing = DIRECTION_ANY;
    players[i].id = i;
  }

  return players;
}

void player_tag(player_t *ctx, uint8_t other_id) {
  ctx->tagged |= (1 << other_id);
}

bool player_is_tagged(player_t *ctx, uint8_t other_id) {
  return (ctx->tagged & (1 << other_id)) > 0;
}

void player_clear_tags(player_t *ctx) { ctx->tagged = 0; }

void player_add_effect(player_t *ctx, struct spell_effect from,
                       spell_effect_value_t value, int duration,
                       const spell_t *spell, player_t *caster) {

  struct player_effect *eff;

  eff = calloc(1, sizeof(*eff));

  eff->eff = from;
  eff->value = value;
  eff->duration = duration;
  eff->spell = spell;
  eff->caster = caster;

  if (ctx->effects == NULL) {
    ctx->effects = eff;
  } else {
    struct player_effect *e;
    for (e = ctx->effects; e->next != NULL; e = e->next)
      ;
    e->next = eff;
  }
}

void player_time_effects(player_t *ctx) {
  ctx->effects = time_effect(ctx->effects);
}

static void player_set_effects(player_t *ctx, struct msg_spell *effects,
                               uint8_t num_effects) {
  struct player_effect *eff;
  struct player_effect *last;
  for (struct player_effect *eff = ctx->effects; eff != NULL; eff = eff->next) {
    eff->duration = 0;
  }
  ctx->effects = time_effect(ctx->effects);

  for (uint8_t i = 0; i < num_effects; i++) {
    eff = calloc(1, sizeof(*eff));

    if (ctx->effects == NULL) {
      ctx->effects = eff;
    } else {
      last->next = eff;
    }
    last = eff;

    eff->duration = effects[i].charges;
    eff->spell = spell_get_by_id(effects[i].id);
  }
}

void player_add_effects_to_msg(player_t *ctx, struct msg_spell **effect,
                               uint8_t *num_effect) {

  *num_effect = 0;
  for (struct player_effect *eff = ctx->effects; eff != NULL; eff = eff->next) {
    *num_effect = *num_effect + 1;
  }
  if (*num_effect == 0) {
    return;
  }
  *effect = malloc(sizeof(**effect) * (*num_effect));
  uint8_t i = 0;
  for (struct player_effect *eff = ctx->effects; eff != NULL; eff = eff->next) {
    (*effect)[i].id = eff->spell->id;
    (*effect)[i].charges = eff->duration;

    i++;
  }
}

void player_batch_update(player_t *ctx, uint32_t num_players, message_t *msg) {
  player_t *me;

  for (uint32_t i = 0; i < num_players; i++) {
    ctx[i].position = POSITION_UNKNOWN;
  }

  for (uint8_t i = 0; i < msg->body.player_update.num_others; i++) {
    player_t *p = &ctx[msg->body.player_update.others[i].player_id];

    p->health = msg->body.player_update.others[i].health;
    p->kills = msg->body.player_update.others[i].kills;
    p->deaths = msg->body.player_update.others[i].deaths;
    p->facing = msg->body.player_update.others[i].face;
    p->position = msg->body.player_update.others[i].pos;
    p->updated = msg->tick;

    for (uint8_t j = 0; j < PORTAL_NONE; j++) {
      p->spells[j] =
          spell_get_by_id(msg->body.player_update.others[i].spells[j].id);
      p->charges[j] = msg->body.player_update.others[i].spells[j].charges;
    }

    player_set_effects(p, msg->body.player_update.others[i].effects,
                       msg->body.player_update.others[i].num_effects);
  }

  me = &ctx[msg->body.player_update.player_id];
  me->health = msg->body.player_update.health;
  me->kills = msg->body.player_update.kills;
  me->deaths = msg->body.player_update.deaths;
  me->facing = msg->body.player_update.face;
  me->position = msg->body.player_update.pos;

  for (uint8_t j = 0; j < PORTAL_NONE; j++) {
    me->spells[j] = spell_get_by_id(msg->body.player_update.spells[j].id);
    me->charges[j] = msg->body.player_update.spells[j].charges;
  }
  player_set_effects(me, msg->body.player_update.effects,
                     msg->body.player_update.num_effects);
}

void player_killed(player_t *ctx) {
  reset(ctx);
  ctx->deaths++;
}

void player_spawn(player_t *ctx, pos_t pos, enum direction facing) {

  enum portal_type spell_kind;

  ctx->facing = facing;
  ctx->position = pos;
  ctx->health = 100;

  spell_kind = rand() % PORTAL_NONE;

  ctx->spells[spell_kind] = spell_get_random(spell_kind);
  ctx->charges[spell_kind] = ctx->spells[spell_kind]->charges;

  printf("PLayer %u got spell %s\n", ctx->id, ctx->spells[spell_kind]->name);
}

void player_client_send_msg(player_t *ctx, message_t *msg) {
  if (ctx->brain.client_send != NULL) {
    ctx->brain.client_send(ctx->brain.client_send_user_data, msg);
  }
}

message_t *player_client_get_msg(player_t *ctx) {
  if (ctx->brain.client_get != NULL) {
    return ctx->brain.client_get(ctx->brain.client_get_user_data);
  }
  return NULL;
}

void player_server_send_msg(player_t *ctx, message_t *msg) {
  if (ctx->brain.server_send != NULL) {
    ctx->brain.server_send(ctx->brain.server_send_user_data, msg);
  }
  if (ctx->brain.client_hook != NULL) {
    ctx->brain.client_hook(msg, ctx->brain.client_hook_user_data);
  }
}

message_t *player_server_get_msg(player_t *ctx) {
  if (ctx->brain.server_get != NULL) {
    return ctx->brain.server_get(ctx->brain.server_get_user_data);
  }
  return NULL;
}
