#pragma once

#include <stdint.h>

#include "common.h"
#include "message.h"
#include "player.h"
#include "spell.h"

typedef struct incident_ctx incident_ctx_t;

typedef struct incident_effect {
  player_t *victim;
  uint8_t type; /*spell effect enum */
  spell_effect_value_t data;
} incident_effect_t;

typedef struct {
  pos_t from;
  pos_t to;
  spell_t *spell;

  player_t *player_origin;

  incident_effect_t *effects;
  uint8_t num_effects;
  uint8_t cap_effects;
} incident_t;

incident_ctx_t *incident_ctx_new(uint32_t capacity);

void incident_ctx_clear(incident_ctx_t *ctx);

incident_t *incident_new(incident_ctx_t *ctx);
incident_effect_t *incident_new_effect(incident_t *ctx);

void incident_add_to_message(incident_ctx_t *ctx, player_t *player,
                             message_t *msg);
