#pragma once

#include <stdint.h>

#include "common.h"
#include "message.h"
#include "player.h"
#include "spell.h"

typedef struct incident_ctx incident_ctx_t;

enum incident_type {
  INCIDENT_SPELL = 0,
  INCIDENT_PORTAL, /* Loading spell from a portal */
  INCIDENT_DELAYED_EFFECT, 
  INCIDENT_PLAYER_KILLED,
  INCIDENT_PLAYER_MOVE
};

typedef struct incident_effect {
  player_t *victim;
  pos_t at;
  uint8_t type; /*spell effect enum */
  spell_effect_value_t data;
  struct incident_effect *next;
} incident_effect_t;

typedef struct incident_target {
  pos_t pos;
  incident_effect_t *effects;
  uint8_t num_effects;
  struct incident_target *next;
} incident_target_t;

typedef struct {
  enum incident_type type;
  pos_t from;
  const spell_t *spell;

  incident_target_t *targets;
  uint8_t num_targets;
  player_t *player_origin;
} incident_t;

incident_ctx_t *incident_ctx_new(uint32_t capacity);

void incident_ctx_clear(incident_ctx_t *ctx);

incident_t *incident_new(incident_ctx_t *ctx);
incident_target_t *incident_new_target(incident_t *incident, pos_t at);
incident_effect_t *incident_new_effect(incident_target_t *target);

void incident_add_to_message(incident_ctx_t *ctx, player_t *player,
                             message_t *msg);

const char *incident_type_string(enum incident_type t);
