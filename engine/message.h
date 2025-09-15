#pragma once

#include <stdint.h>

#include "common.h"
#include "spell.h"

enum message_type {
  MESSAGE_ASK_READY,
  MESSAGE_REPLY_READY,
  MESSAGE_MAP,
  MESSAGE_REPLY_MAP,
  MESSAGE_ASK_SPAWN,
  MESSAGE_REPLY_SPAWN,
  MESSAGE_ASK_MOVE,
  MESSAGE_REPLY_MOVE,
  MESSAGE_ASK_FIGHT,
  MESSAGE_REPLY_FIGHT,
  MESSAGE_PLAYER_UPDATE,
  MESSAGE_REPLY_PLAYER_UPDATE,
  MESSAGE_REPORT,
  MESSAGE_REPLY_REPORT
};

struct msg_opts {
  pos_t *opts;
  uint32_t size;
};

struct msg_spell {
  uint8_t id;
  uint8_t charges;
};

struct msg_portal_kind {
  pos_t pos;
  uint8_t kind;
};

struct applied_effect {
  uint8_t type; /*spell effect enum */
  uint8_t victim;
  pos_t at;
  int8_t duration;
  spell_effect_value_t data;
};

struct target {
  pos_t target;
  struct applied_effect *effects;
  uint8_t num_effects;
};

struct incident {
  pos_t from;
  uint8_t incident_type;
  uint8_t spell_kind;
  
  /* Contains the following if caster or target is in LoS */
  uint8_t spell_id;
  uint8_t player_origin;

  struct target *targets;
  uint8_t num_targets;
};

typedef struct {
  enum message_type type;
  uint32_t tick;
  union {
    struct {
      coord_t width;
      coord_t height;
      uint8_t *data;

      uint8_t num_players;
      uint8_t num_portals;
      struct msg_portal_kind *portals;
    } map;

    struct {
      pos_t *opts;
      uint32_t size;
      uint8_t player_id;
    } ask_spawn;

    struct {
      pos_t dst;
      uint8_t face;
    } reply_spawn;

    struct msg_opts ask_move;

    struct {
      pos_t dst;
      uint8_t face;
    } reply_move;

    struct {
      uint8_t spell_id[PORTAL_NONE];
      struct msg_opts spell_opts[PORTAL_NONE];
    } ask_fight;

    struct {
      uint8_t spell_id;
      pos_t target;
    } reply_fight;

    struct {
      uint8_t player_id;
      pos_t pos;
      uint8_t face;
      int8_t health;
      int8_t kills;
      int8_t deaths;

      struct msg_spell spells[PORTAL_NONE];
      struct msg_opts los;

      /* Other players that are within LOS */
      struct {
        uint8_t player_id;
        pos_t pos;
        uint8_t face;
        int8_t health;
        int8_t kills;
        int8_t deaths;

        struct msg_spell spells[PORTAL_NONE];
      } *others;
      uint8_t num_others;

      struct {
        uint8_t kind;
        uint8_t spell;
        pos_t pos;
      } *portals;
      uint8_t num_portals;

      struct incident *events;
      uint32_t num_events;

    } player_update;

    struct {
    } report;

  } body;
} message_t;

message_t *message_ask_ready(uint32_t tick);
message_t *message_reply_ready(uint32_t tick);

message_t *message_map(uint32_t tick);
message_t *message_reply_map(uint32_t tick);

message_t *message_ask_spawn(uint32_t tick, uint8_t player_id, uint32_t size,
                             pos_t *options);
message_t *message_reply_spawn(uint32_t tick, pos_t pos, uint8_t facing);

message_t *message_ask_move(uint32_t tick, uint32_t size, pos_t *options);
message_t *message_reply_move(uint32_t tick, pos_t pos, uint8_t facing);

message_t *message_ask_fight(uint32_t tick);
message_t *message_reply_fight(uint32_t tick, uint8_t spell_id, pos_t target);

message_t *message_player_update(uint32_t tick);
message_t *message_reply_player_update(uint32_t tick);

message_t *message_report(uint32_t tick);
message_t *message_reply_report(uint32_t tick);
message_t *message_ref(message_t *msg);
void message_unref(message_t *msg);
