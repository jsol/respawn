#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "spell.h"

static spell_t water[] =
    {{
         .name = "Whip",
         .speed = 75,
         .charges = 5,
         .burst = 1,
         .range = {{
                       .range = 10,
                       .hit = 40,
                       .dmg = {.min = 30, .max = 50},
                   },
                   {
                       .range = 20,
                       .hit = 30,
                       .dmg = {.min = 20, .max = 50},
                   },
                   {
                       .range = 30,
                       .hit = 10,
                       .dmg = {.min = 10, .max = 50},
                   }

         },
         .effect = {{
             .type = SPELL_EFFECT_PUSH,
             .params = {.move = {.min = 0, .max = 3}},
         }},
     },
     {.name = "Ooze",
      .speed = 60,
      .charges = 5,
      .burst = 2,
      .range = {{
                    .range = 20,
                    .hit = 50,
                    .dmg = {.min = 10, .max = 25},
                },
                {
                    .range = 30,
                    .hit = 30,
                    .dmg = {.min = 10, .max = 25},
                },
                {
                    .range = 40,
                    .hit = 15,
                    .dmg = {.min = 10, .max = 25},
                }

      },
      .effect =
          {
              {.type = SPELL_EFFECT_POISON,
               .params = {.poison = {.min = 5, .max = 20, .duration = 3}}},
          }},

     {.name = "Hammer",
      .speed = 5,
      .charges = 2,
      .burst = 1,
      .range = {{
          .range = 7,
          .hit = 80,
          .dmg = {.min = 80, .max = 100},
      }

      }},
     {.name = "Baptize",
      .defencive = true,
      .speed = 5,
      .charges = 2,
      .burst = 1,
      .range = {{
          .range = 10,
          .hit = 100,
          .dmg = {.min = 0, .max = 10},
      }},
      .effect =
          {
              {.type = SPELL_EFFECT_HEAL,
               .params = {.heal = {.min = 10, .max = 30}}

              },
              {.type = SPELL_EFFECT_DAMAGE_MOD,
               .params = {.mod = {.value = -5, .duration = 2}}},
          }}

};

static spell_t
    earth[] = {{
                   .name = "Boulder",
                   .speed = 20,
                   .charges = 3,
                   .burst = 1,
                   .range = {{
                                 .range = 5,
                                 .hit = 80,
                                 .dmg = {.min = 60, .max = 70},
                             },
                             {
                                 .range = 10,
                                 .hit = 50,
                                 .dmg = {.min = 60, .max = 70},
                             },
                             {
                                 .range = 15,
                                 .hit = 30,
                                 .dmg = {.min = 60, .max = 80},
                             },
                             {
                                 .range = 20,
                                 .hit = 10,
                                 .dmg = {.min = 60, .max = 80},
                             }

                   },
               },
               {.name = "Pebbles",
                .speed = 95,
                .charges = 15,
                .burst = 5,
                .range = {{
                              .range = 10,
                              .hit = 20,
                              .dmg = {.min = 15, .max = 20},
                          },
                          {
                              .range = 20,
                              .hit = 15,
                              .dmg = {.min = 10, .max = 15},
                          },
                          {
                              .range = 30,
                              .hit = 10,
                              .dmg = {.min = 5, .max = 10},
                          },
                          {
                              .range = 40,
                              .hit = 5,
                              .dmg = {.min = 0, .max = 10},
                          }

                }

               },
               {.name = "Iron suit",
                .speed = 100,
                .defencive = true,
                .charges = 3,
                .burst = 1,
                .range = {{
                    .range = 10,
                    .hit = 100,
                    .dmg = {.min = 0, .max = 0},
                }},
                .effect = {{.type = SPELL_EFFECT_BE_HIT_MOD,
                            .params = {.mod = {.value = 5, .duration = 3}}},
                           {.type = SPELL_EFFECT_DAMAGE_MOD,
                            .params = {.mod = {.value = -15, .duration = 3}}}}

               },
               {.name = "Grenade",
                .speed = 20,
                .charges = 3,
                .burst = 1,
                .miss = SPELL_MISS_BOUNCE,
                .bounce_max = 15,
                .range = {{
                              .range = 10,
                              .hit = 80,
                              .dmg = {.min = 30, .max = 50},
                          },
                          {
                              .range = 20,
                              .hit = 40,
                              .dmg = {.min = 30, .max = 50},
                          },
                          {
                              .range = 30,
                              .hit = 20,
                              .dmg = {.min = 30, .max = 50},
                          }},
                .effect =
                    {
                        {.type = SPELL_EFFECT_SPLASH,
                         .params =
                             {.splash =
                                  {.radius_step = 1,
                                   .drop_of = 5,
                                   .dmg = {.min = 15, .max = 25}}}},
                        {.type = SPELL_EFFECT_SPLASH,
                         .params = {.splash = {.radius_step = 2,
                                               .drop_of = 10,
                                               .dmg = {.min = 15, .max = 25}}}},
                        {.type = SPELL_EFFECT_SPLASH,
                         .params = {.splash = {.radius_step = 1,
                                               .drop_of = 20,
                                               .dmg = {.min = 15, .max = 45}}}},
                        {.type = SPELL_EFFECT_PUSH_RANDOM,
                         .params = {.move = {.min = 1, .max = 3}}},
                    }

               }

};

static spell_t
    air[] =
        {
            {.name = "Swipe",
             .speed = 85,
             .charges = 4,
             .burst = 1,
             .range =
                 {
                     {
                         .range = 40,
                         .hit = 80,
                         .dmg = {.min = 10, .max = 60},
                     },
                 },
             .effect = {{.type = SPELL_EFFECT_PUSH_RANDOM,
                         .params = {.move = {.min = 3, .max = 6}}}}},
            {.name = "Cannon",
             .speed = 80,
             .charges = 3,
             .burst = 1,
             .range = {{
                           .range = 10,
                           .hit = 80,
                           .dmg = {.min = 20, .max = 50},
                       },
                       {
                           .range = 30,
                           .hit = 70,
                           .dmg = {.min = 10, .max = 40},
                       },
                       {
                           .range = 40,
                           .hit = 60,
                           .dmg = {.min = 5, .max = 35},
                       },
                       {
                           .range = 50,
                           .hit = 50,
                           .dmg = {.min = 5, .max = 30},
                       }

             },
             .effect = {{.type = SPELL_EFFECT_PUSH,
                         .params = {.move = {.min = 3, .max = 3}}}}},

            {.name = "Choke",
             .speed = 30,
             .charges = 2,
             .burst = 1,
             .range =
                 {
                     {
                         .range = 5,
                         .hit = 70,
                         .dmg = {.min = 5, .max = 20},
                     },
                     {
                         .range = 15,
                         .hit = 60,
                         .dmg = {.min = 5, .max = 20},
                     },

                 },
             .effect =
                 {
                     {.type = SPELL_EFFECT_BE_HIT_MOD,
                      .params = {.mod = {.value = 10, .duration = 4}}},
                     {.type = SPELL_EFFECT_POISON,
                      .params =
                          {.poison = {.min = 10, .max = 25, .duration = 4}}},
                 }

            },

            {.name = "Buffet",
             .speed = 55,
             .charges = 5,
             .burst = 3,
             .range = {{
                           .range = 10,
                           .hit = 50,
                           .dmg = {.min = 5, .max = 20},
                       },
                       {
                           .range = 20,
                           .hit = 35,
                           .dmg = {.min = 5, .max = 15},
                       },
                       {
                           .range = 30,
                           .hit = 10,
                           .dmg = {.min = 5, .max = 10},
                       },
                       {
                           .range = 35,
                           .hit = 5,
                           .dmg = {.min = 0, .max = 10},
                       }

             },
             .effect =
                 {{.type = SPELL_EFFECT_HIT_MOD,
                   .params = {.mod = {.value = -10, .duration = 2}}},
                  {.type = SPELL_EFFECT_POISON,
                   .params = {.poison = {.min = 1, .max = 10, .duration = 2}}},
                  {.type = SPELL_EFFECT_PUSH_RANDOM,
                   .params = {.move = {.min = 0, .max = 3}}}}}

};

static spell_t fire[] = {
    {
        .name = "Lance",
        .speed = 25,
        .charges = 3,
        .burst = 1,
        .range = {{
                      .range = 15,
                      .hit = 20,
                      .dmg = {.min = 60, .max = 100},
                  },
                  {
                      .range = 25,
                      .hit = 50,
                      .dmg = {.min = 50, .max = 100},
                  },
                  {
                      .range = 45,
                      .hit = 60,
                      .dmg = {.min = 40, .max = 100},
                  },
                  {
                      .range = 60,
                      .hit = 65,
                      .dmg = {.min = 30, .max = 80},
                  }},
    },
    {.name = "Fireball",
     .speed = 55,
     .charges = 5,
     .burst = 1,
     .range = {{
                   .range = 10,
                   .hit = 80,
                   .dmg = {.min = 40, .max = 60},
               },
               {
                   .range = 30,
                   .hit = 50,
                   .dmg = {.min = 40, .max = 60},
               },
               {
                   .range = 50,
                   .hit = 30,
                   .dmg = {.min = 40, .max = 60},
               }},
     .effect = {{.type = SPELL_EFFECT_SPLASH,
                 .params.splash = {.radius_step = 2,
                                   .drop_of = 3,
                                   .dmg = {.min = 5, .max = 20}}}

     }},
    {.name = "On Fire",
     .speed = 85,
      .defencive = true,
     .charges = 1,
     .burst = 1,
     .range = {{
         .range = 5,
         .hit = 100,
         .dmg = {.min = 0, .max = 20},
     }},
     .effect = {{.type = SPELL_EFFECT_SPLASH,
                 .params.splash = {.radius_step = 1,
                                   .drop_of = 4,
                                   .dmg = {.min = 5, .max = 30}}},
                {.type = SPELL_EFFECT_HIT_MOD,
                 .params.mod = {.value = 10, .duration = 5}},
                {.type = SPELL_EFFECT_BE_HIT_MOD,
                 .params.mod = {.value = 5, .duration = 5}},
                {.type = SPELL_EFFECT_DAMAGE_MOD,
                 .params.mod = {.value = -10, .duration = 5}},
                {.type = SPELL_EFFECT_POISON,
                 .params.poison = {.min = 5, .max = 10, .duration = 3}}

     }

    },
    {.name = "Torch",
     .speed = 25,
     .charges = 3,
     .burst = 1,
     .range = {{
                   .range = 10,
                   .hit = 75,
                   .dmg = {.min = 0, .max = 20},
               },
               {
                   .range = 15,
                   .hit = 60,
                   .dmg = {.min = 0, .max = 20},
               },
               {
                   .range = 20,
                   .hit = 40,
                   .dmg = {.min = 0, .max = 20},
               }},
     .effect = {{.type = SPELL_EFFECT_SPLASH,
                 .params.splash = {.radius_step = 1,
                                   .drop_of = 5,
                                   .dmg = {.min = 10, .max = 40}}},
                {.type = SPELL_EFFECT_BE_HIT_MOD,
                 .params.mod = {.value = 5, .duration = 2}},
                {.type = SPELL_EFFECT_DAMAGE_MOD,
                 .params.mod = {.value = 5, .duration = 2}},
                {.type = SPELL_EFFECT_POISON,
                 .params.poison = {.min = 20, .max = 40, .duration = 2}}

     }

    }

};

static const spell_t **by_id = NULL;
static void admin(spell_t *s, enum portal_type kind, uint8_t num) {
  static uint8_t id = 0;

  for (uint8_t i = 0; i < num; i++) {
    uint8_t max_ranges = sizeof(s[i].range) / sizeof(struct spell_range);
    uint8_t max_effects = sizeof(s[i].effect) / sizeof(struct spell_effect);

    if (s[i].id == 0) {
      id++;
      s[i].id = id;

      s[i].num_ranges = 0;
      for (uint8_t j = 0; j < max_ranges; j++) {
        if (s[i].range[j].range >= s[i].max_range) {
          s[i].num_ranges++;
          s[i].max_range = s[i].range[j].range;
        }
      }
      s[i].num_effects = 0;
      for (uint8_t j = 0; j < max_effects; j++) {
        if (s[i].effect[j].type > 0) {
          s[i].num_effects++;
        }
      }

      printf("Setting up %s with max range %d and num_ranges %u\n", s[i].name,
             s[i].max_range, s[i].num_ranges);
    }
    s[i].kind = kind;
    if (by_id != NULL) {
      by_id[id] = &s[i];
    }
  }
}

const spell_t *spell_get_kind(enum portal_type type, uint8_t *num_spells) {

  switch (type) {
  case PORTAL_WATER:
    *num_spells = sizeof(water) / sizeof(spell_t);
    admin(water, type, *num_spells);
    return water;
  case PORTAL_EARTH:
    *num_spells = sizeof(earth) / sizeof(spell_t);
    admin(earth, type, *num_spells);
    return earth;
  case PORTAL_AIR:
    *num_spells = sizeof(air) / sizeof(spell_t);
    admin(air, type, *num_spells);
    return air;
  case PORTAL_FIRE:
    admin(fire, type, *num_spells);
    *num_spells = sizeof(fire) / sizeof(spell_t);
    return fire;
  default:
    *num_spells = 0;
    return NULL;
  }
}

void spell_init(void) {
  uint8_t num;

  if (by_id == NULL) {
    uint8_t spell_count = 0;

    spell_count += sizeof(water) / sizeof(spell_t);
    spell_count += sizeof(earth) / sizeof(spell_t);
    spell_count += sizeof(air) / sizeof(spell_t);
    spell_count += sizeof(fire) / sizeof(spell_t);

    /* id 0 is always no spell */
    by_id = calloc(spell_count + 1, sizeof(spell_t));
  }

  spell_get_kind(PORTAL_WATER, &num);
  spell_get_kind(PORTAL_EARTH, &num);
  spell_get_kind(PORTAL_AIR, &num);
  spell_get_kind(PORTAL_FIRE, &num);
}

const spell_t *spell_get_random(enum portal_type type) {
  uint8_t num;
  switch (type) {
  case PORTAL_WATER:
    num = sizeof(water) / sizeof(spell_t);
    return &water[rand() % num];
  case PORTAL_EARTH:
    num = sizeof(earth) / sizeof(spell_t);
    return &earth[rand() % num];
  case PORTAL_AIR:
    num = sizeof(air) / sizeof(spell_t);
    return &air[rand() % num];
  case PORTAL_FIRE:
    num = sizeof(fire) / sizeof(spell_t);
    return &fire[rand() % num];
  default:
    return NULL;
  }
}

const spell_t *spell_get_by_id(uint8_t id) {

  if (by_id == NULL || id == 0) {
    return NULL;
  }

  return by_id[id];
}
const char *spell_id_to_name(uint8_t id) {
  if (id == 0 || by_id == NULL) {
    return "Unknown spell";
  }

  return by_id[id]->name;
}

static inline void set_int(int8_t *dst, int8_t val) {
  if (dst != NULL) {
    *dst = val;
  }
}

void spell_get_stats(const spell_t *spell, coord_t distance_squared,
                     int8_t *hit, int8_t *dmg_min, int8_t *dmg_max) {

  for (uint i = 0; i < spell->num_ranges; i++) {

    if (distance_squared <= (spell->range[i].range * spell->range[i].range)) {
      set_int(dmg_min, spell->range[i].dmg.min);
      set_int(dmg_max, spell->range[i].dmg.max);
      set_int(hit, spell->range[i].hit);
      return;
    }
  }
  set_int(dmg_min, 0);
  set_int(dmg_max, 0);
  set_int(hit, 0);
}
