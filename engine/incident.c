#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "incident.h"
#include "message.h"
#include "player.h"
#include "spell.h"

struct incident_ctx {
  uint32_t size;
  uint32_t capacity;
  incident_t *data;
};

incident_ctx_t *incident_ctx_new(uint32_t capacity) {
  incident_ctx_t *ctx;

  ctx = malloc(sizeof(*ctx));
  ctx->size = 0;
  ctx->capacity = capacity;
  ctx->data = calloc(capacity, sizeof(*ctx->data));

  return ctx;
}

static bool check_effects(incident_t *inc, player_t *player) {
  for (uint8_t j = 0; j < inc->num_effects; j++) {
    if (inc->player_origin->id == player->id ||
        player_is_tagged(player, inc->player_origin->id) ||
        map_opts_contains(player->los, inc->player_origin->position)) {
      return true;
    }
  }

  return false;
}

void incident_ctx_clear(incident_ctx_t *ctx) { ctx->size = 0; }

void incident_add_to_message(incident_ctx_t *ctx, player_t *player,
                             message_t *msg) {
  msg->body.player_update.events =
      malloc(ctx->size * sizeof(*msg->body.player_update.events));
  msg->body.player_update.num_events = 0;

  for (uint32_t i = 0; i < ctx->size; i++) {
    incident_t *inc = &ctx->data[i];

    msg->body.player_update.events[i].from = inc->from;
    msg->body.player_update.events[i].spell_kind = inc->spell->kind;

    if (inc->player_origin->id == player->id ||
        map_opts_contains(player->los, inc->player_origin->position)) {

      msg->body.player_update.events[i].player_origin = inc->player_origin->id;
      msg->body.player_update.events[i].spell_id = inc->spell->id;
      msg->body.player_update.events[i].to = inc->to;
    } else if (inc->player_origin->id == player->id ||
               map_opts_contains(player->los, inc->to)) {

      msg->body.player_update.events[i].spell_id = inc->spell->id;
      msg->body.player_update.events[i].to = inc->to;
    }

    msg->body.player_update.events[i].effects = malloc(
        inc->num_effects * sizeof(*msg->body.player_update.events[i].effects));
    msg->body.player_update.events[i].num_effects = 0;

    for (uint32_t j = 0; j < inc->num_effects; j++) {
      if (inc->effects->victim != NULL &&
          map_opts_contains(player->los, inc->effects->victim->position)) {
        struct applied_effect *eff =
            &msg->body.player_update.events[i]
                 .effects[msg->body.player_update.events[i].num_effects];

        eff->victim = inc->effects[j].victim->id;
        eff->type = inc->effects[j].type;
        eff->data = inc->effects[j].data;

        msg->body.player_update.events[i].num_effects++;
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
      ctx->data[i].effects = NULL;
    }
  }

  inc = &ctx->data[ctx->size];

  inc->to = POSITION_UNKNOWN;
  inc->from = POSITION_UNKNOWN;
  inc->player_origin = NULL;
  inc->spell = NULL;
  inc->num_effects = 0;

  if (inc->effects == NULL) {
    inc->cap_effects = 2;
    inc->effects = malloc(inc->cap_effects * sizeof(*inc->effects));
  }

  ctx->size++;
  return inc;
}

incident_effect_t *incident_new_effect(incident_t *ctx) {
  incident_effect_t *eff;

  if (ctx->num_effects == ctx->cap_effects) {
    ctx->cap_effects = ctx->cap_effects * 2;
    // TODO: Fix NULL return
    ctx->effects =
        realloc(ctx->effects, sizeof(*ctx->effects) * ctx->cap_effects);
  }

  eff = &ctx->effects[ctx->num_effects];
  memset(eff, 0, sizeof(*eff));

  ctx->num_effects++;
  return eff;
}
