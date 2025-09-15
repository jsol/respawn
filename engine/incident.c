#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "incident.h"
#include "map_opts.h"
#include "message.h"
#include "player.h"
#include "spell.h"

struct incident_ctx {
  uint32_t size;
  uint32_t capacity;
  incident_t *data;
};

static void free_effect(incident_effect_t *eff) {
  if (eff == NULL) {
    return;
  }

  free_effect(eff->next);
  free(eff);
}

static void free_target(incident_target_t *t) {
  if (t == NULL) {
    return;
  }

  free_effect(t->effects);
  free_target(t->next);
  free(t);
}

incident_ctx_t *incident_ctx_new(uint32_t capacity) {
  incident_ctx_t *ctx;

  ctx = malloc(sizeof(*ctx));
  ctx->size = 0;
  ctx->capacity = capacity;
  ctx->data = calloc(capacity, sizeof(*ctx->data));

  return ctx;
}

void incident_ctx_clear(incident_ctx_t *ctx) {

  for (uint32_t i = 0; i < ctx->size; i++) {
    free_target(ctx->data[i].targets);
    ctx->data[i].targets = NULL;
    ctx->data[i].num_targets = 0;
  }

  ctx->size = 0;
}

static bool add_target_to_msg(struct target *dst, incident_target_t *from,
                              bool caster_seen, player_t *observer) {
  bool ret = false;
  if (caster_seen) {
    ret = true;
    dst->target = from->pos;
  }

  dst->num_effects = 0;

  if (observer->health <= 0 || map_opts_contains(observer->los, from->pos)) {
    uint8_t *c = NULL;

    dst->target = from->pos;
    dst->effects = calloc(from->num_effects, sizeof(*dst->effects));
    dst->num_effects = 0;

    c = &dst->num_effects;

    for (incident_effect_t *eff = from->effects; eff != NULL; eff = eff->next) {

      if (observer->health <= 0 || map_opts_contains(observer->los, eff->at)) {
        dst->effects[*c].type = eff->type;
        dst->effects[*c].data = eff->data;
        dst->effects[*c].at = eff->at;
        dst->effects[*c].victim = eff->victim->id;
        *c = *c + 1;
      }
    }
  }

  return ret;
}

void incident_add_to_message(incident_ctx_t *ctx, player_t *player,
                             message_t *msg) {
  msg->body.player_update.events =
      malloc(ctx->size * sizeof(*msg->body.player_update.events));
  msg->body.player_update.num_events = ctx->size;
  printf("Adding %u events to message for player %u\n", ctx->size, player->id);
  uint8_t *counter = NULL;

  for (uint32_t i = 0; i < ctx->size; i++) {
    incident_t *inc = &ctx->data[i];
    bool add_effects = false;

    msg->body.player_update.events[i].incident_type = inc->type;
    msg->body.player_update.events[i].from = inc->from;
    msg->body.player_update.events[i].spell_id = 0;
    msg->body.player_update.events[i].spell_kind = 0;
    msg->body.player_update.events[i].player_origin = PLAYER_UNKNOWN;
    if (inc->spell != NULL) {
      msg->body.player_update.events[i].spell_kind = inc->spell->kind;
    }

    if ((inc->player_origin != NULL && inc->player_origin->id == player->id) || player->health <= 0 ||
        map_opts_contains(player->los, inc->from)) {

      if (inc->player_origin != NULL) {
        msg->body.player_update.events[i].player_origin =
            inc->player_origin->id;
      }
      if (inc->spell != NULL) {
        msg->body.player_update.events[i].spell_id = inc->spell->id;
      }
      add_effects = true;
    }

    msg->body.player_update.events[i].targets = calloc(
        inc->num_targets, sizeof(*msg->body.player_update.events[i].targets));
    msg->body.player_update.events[i].num_targets = 0;
    counter = &msg->body.player_update.events[i].num_targets;

    for (incident_target_t *t = inc->targets; t != NULL; t = t->next) {
      printf("Processing effect\n");
      if (add_target_to_msg(
              &msg->body.player_update.events[i].targets[*counter], t,
              add_effects, player)) {
        *counter += 1;
      }

      /* TODO: Check if non-player-targeted effects should be sent here */
    }
  }
}

incident_t *incident_new(incident_ctx_t *ctx) {
  incident_t *inc;
  if (ctx->size == ctx->capacity) {
    ctx->capacity = ctx->capacity * 2;
    // TODO: Fix NULL return
    ctx->data = realloc(ctx->data, sizeof(*ctx->data) * ctx->capacity);

    for (uint32_t i = ctx->size; i < ctx->capacity; i++) {
      ctx->data[i].targets = NULL;
    }
  }

  inc = &ctx->data[ctx->size];

  inc->from = POSITION_UNKNOWN;
  inc->player_origin = NULL;
  inc->spell = NULL;
  inc->num_targets = 0;
  inc->targets = NULL;

  ctx->size++;
  return inc;
}

incident_target_t *incident_new_target(incident_t *ctx, pos_t at) {
  incident_target_t *t;
  incident_target_t *old;

  t = calloc(1, sizeof(*t));

  t->pos = at;

  if (ctx->targets == NULL) {
    ctx->targets = t;
  } else {
    for (old = ctx->targets; old->next != NULL; old = old->next)
      ;
    old->next = t;
  }

  ctx->num_targets++;

  return t;
}

incident_effect_t *incident_new_effect(incident_target_t *ctx) {
  incident_effect_t *eff;
  incident_effect_t *old;

  eff = calloc(1, sizeof(*eff));
  if (ctx->effects == NULL) {
    ctx->effects = eff;
  } else {
    for (old = ctx->effects; old->next != NULL; old = old->next)
      ;

    old->next = eff;
  }

  ctx->num_effects++;
  return eff;
}
const char *incident_type_string(enum incident_type t) {
  switch (t) {
  case INCIDENT_SPELL:
    return "Spell";
  case INCIDENT_PORTAL:
    return "Portal";
  case INCIDENT_DELAYED_EFFECT:
    return "Delayed effect";
  case INCIDENT_PLAYER_KILLED:
    return "PLAYER KILLED";
  }

  return "Unknown";
}
