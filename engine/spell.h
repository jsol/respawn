#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "common.h"

enum spell_miss {
  SPELL_MISS_LOS, /* Assigns random target square close to target, but hits on
                     the Line of sight to that square. Default. */
  SPELL_MISS_BOUNCE,    /* Bounce to random square close to target */
  SPELL_MISS_INTERRUPT, /* Stops the ongoing attack if burst > 1 */
  SPELL_MISS_NONE
};

enum spell_activation { SPELL_ACTIVATION_ATTACK = 0, SPELL_ACTIVATION_MOVE };

enum spell_effect_types {
  SPELL_EFFECT_NONE = 0,
  SPELL_EFFECT_DAMAGE, /* calculated, for reporting any damage */
  SPELL_EFFECT_SPLASH,
  SPELL_EFFECT_PUSH,
  SPELL_EFFECT_PULL,
  SPELL_EFFECT_PUSH_RANDOM,
  SPELL_EFFECT_POISON,
  SPELL_EFFECT_OBSCURE, /* Make target position break line of sight */
  SPELL_EFFECT_HEAL,
  SPELL_EFFECT_DAMAGE_MOD, /* Modify damage taken in the future */
  SPELL_EFFECT_HIT_MOD,    /* Modify chance to hit something in the future */
  SPELL_EFFECT_BE_HIT_MOD  /* Modify chance to be hit in the future */
};

typedef union {
  int8_t dmg;
  pos_t new_pos;
  struct {
    pos_t center;
    coord_t radius;
  } area;
  int8_t duration;
} spell_effect_value_t;

struct spell_range {
  coord_t range;
  int8_t hit;
  struct {
    int8_t min;
    int8_t max;
  } dmg;
};

struct spell_effect {
  enum spell_effect_types type;
  union {
    struct {
      int8_t value;
      int8_t duration;
    } mod;
    struct {
      coord_t min;
      coord_t max;
    } move;
    struct {
      int8_t radius_step; /* For each radius step dmg is reduced by drop_of */
      int8_t drop_of;
      struct {
        int8_t min;
        int8_t max;
      } dmg;
    } splash;
    struct {
      int8_t min;
      int8_t max;
    } heal;
    struct {
      int8_t min;
      int8_t max;
      int8_t duration;
    } poison;
  } params;
};

typedef struct {
  uint8_t id;
  enum portal_type kind;
  const char *name;
  bool defencive; /* just to help the NPC */
  enum spell_activation activation;
  uint8_t speed; /* Highest shoots first */
  int8_t charges;

  struct spell_range range[5];
  int8_t num_ranges; /* computed */
  coord_t max_range; /* computed */

  int8_t burst; /* number of shots per activation */

  enum spell_miss miss;
  int32_t bounce_max;

  struct spell_effect effect[5];
  int8_t num_effects; /* computed */

} spell_t;

void spell_init(void);
const spell_t *spell_get_kind(enum portal_type, uint8_t *num_spells);
const spell_t *spell_get_random(enum portal_type);
const spell_t *spell_get_by_id(uint8_t id);
const char *spell_id_to_name(uint8_t id);
void spell_get_stats(const spell_t *spell, coord_t distance_squared,
                     int8_t *hit, int8_t *dmg_min, int8_t *dmg_max);
