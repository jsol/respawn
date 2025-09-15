#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "engine.h"
#include "incident.h"
#include "map.h"
#include "map_opts.h"
#include "message.h"
#include "player.h"
#include "portals.h"
#include "spell.h"

typedef enum state {
  STATE_STARTING = 0,
  STATE_WAIT_READY,
  STATE_WAIT_MAP,
  STATE_WAIT_INIT_SPAWN,
  STATE_WAIT_PLAYER_UPDATE_ACK,
  STATE_ASK_MOVE,
  STATE_WAIT_MOVE,
  STATE_WAIT_MOVE_PLAYER_UPDATE_ACK,
  STATE_ASK_FIGHT,
  STATE_WAIT_FIGHT,
  STATE_WAIT_FIGHT_PLAYER_UPDATE_ACK,
} state_t;

struct waiting {
  enum message_type type;
  uint32_t tick;
  message_t *incoming;
  message_t *sent;
};

struct engine_ctx {
  portals_ctx_t *portals;
  player_t *players;
  uint8_t player_count;
  struct waiting *waiting;
  incident_ctx_t *incidents;
  state_t state;

  uint8_t init_spawn_active;

  uint32_t tick;
  uint32_t turns;
  bool timer;

  map_t *map;
};

static void setup_portals(engine_t *ctx) {
  map_opts_t *portals;

  portals = map_valid_spawns(ctx->map, 16, 15);

  for (uint32_t i = 0; i < portals->size; i++) {
    map_set_portal(ctx->map, portals->data[i]);
    portals_add_kind(ctx->portals, i % PORTAL_NONE, portals->data[i]);
  }
}

engine_t *engine_new(uint8_t num_players, map_t *map, portals_ctx_t *portals,
                     bool timer) {
  engine_t *ctx;

  ctx = malloc(sizeof(*ctx));

  ctx->players = player_create(num_players);
  ctx->player_count = num_players;
  ctx->timer = timer;
  ctx->map = map;
  ctx->portals = portals;
  ctx->incidents = incident_ctx_new(num_players);
  ctx->tick = 0;
  ctx->turns = 0;

  if (portals == NULL) {
    ctx->portals = portals_new(16);
    setup_portals(ctx);
  }

  ctx->state = STATE_STARTING;
  ctx->waiting = calloc(sizeof(*ctx->waiting), num_players);

  return ctx;
}

static void player_position_update(engine_t *ctx, uint8_t player_id,
                                   pos_t to_pos, enum direction face) {
  player_t *p = &ctx->players[player_id];

  if (POS_EQ(p->position, to_pos)) {
    return;
  }

  map_unset_player(ctx->map, p->position);
  p->position = to_pos;

  for (uint8_t i = 0; i < ctx->player_count; i++) {
    map_set_player(ctx->map, ctx->players[i].position);
  }

  if (face != DIRECTION_ANY) {
    p->facing = face;
    map_opts_free(p->los);
    p->los = map_line_of_sight(ctx->map, p->position, p->facing);
  }
}

static void send_all(engine_t *ctx, message_t *msg) {

  for (uint8_t i = 0; i < ctx->player_count; i++) {
    player_server_send_msg(&ctx->players[i], msg);
  }
}

static bool players_ready(engine_t *ctx) {
  message_t *msg;

  for (uint8_t i = 0; i < ctx->player_count; i++) {
    if (ctx->waiting[i].tick == 0) {
      continue;
    }
    msg = player_server_get_msg(&ctx->players[i]);
    if (msg != NULL) {
      printf("GOT MESSAGE TYPE %u, TICK %u FROM %u\n", msg->type, msg->tick, i);
    }
    if (msg != NULL && msg->type == ctx->waiting[i].type &&
        ctx->waiting[i].tick == msg->tick) {
      ctx->waiting[i].tick = 0;
      ctx->waiting[i].incoming = msg;
      continue;
    }
    message_unref(msg);
    return false;
  }
  return true;
}

static void resolve_deaths(engine_t *ctx) {
  for (uint8_t i = 0; i < ctx->player_count; i++) {
    incident_t *inc;
    player_t *p = &ctx->players[i];

    if (p->health > 0) {
      p->injured_by = 0;
      continue;
    }

    for (uint8_t j = 0; j < ctx->player_count; j++) {
      if (p->injured_by & (1 << j)) {
        if (j == i) {
          p->kills--;
        } else {
          ctx->players[j].kills++;
        }
      }
    }

    inc = incident_new(ctx->incidents);
    inc->type = INCIDENT_PLAYER_KILLED;
    inc->from = p->position;
    inc->player_origin = p;

    player_killed(p);
    player_position_update(ctx, p->id, POSITION_UNKNOWN, DIRECTION_ANY);
  }
}

static void clear_waiting(struct waiting *w) {
  w->tick = 0;
  message_unref(w->sent);
  message_unref(w->incoming);
  w->sent = NULL;
  w->incoming = NULL;
}

static void players_wait(engine_t *ctx, enum message_type type) {
  for (uint8_t i = 0; i < ctx->player_count; i++) {
    clear_waiting(&ctx->waiting[i]);
    ctx->waiting[i].tick = ctx->tick;
    ctx->waiting[i].type = type;
  }
}

static void ask_spawn(engine_t *ctx, uint8_t id) {
  message_t *msg;
  map_opts_t *points;

  clear_waiting(&ctx->waiting[id]);
  ctx->waiting[id].tick = ctx->tick;
  ctx->waiting[id].type = MESSAGE_REPLY_SPAWN;

  points = map_valid_spawns(ctx->map, 3, 15);

  msg = message_ask_spawn(ctx->tick, id, points->size, points->data);
  ctx->waiting[id].sent = msg;
  player_server_send_msg(&ctx->players[id], msg);

  map_opts_free(points);
}

static bool spawn_reply(engine_t *ctx, uint8_t id) {
  bool ok = false;
  pos_t pos;
  enum direction facing;

  struct waiting *w = &ctx->waiting[id];

  if (w->incoming == NULL || w->incoming->type != MESSAGE_REPLY_SPAWN) {

    return false;
  }

  for (uint32_t i = 0; i < w->sent->body.ask_spawn.size; i++) {
    if (POS_EQ(w->incoming->body.reply_spawn.dst,
               w->sent->body.ask_spawn.opts[i])) {
      ok = true;
      break;
    }
  }

  if (ok) {
    pos = w->incoming->body.reply_spawn.dst;
  } else {
    pos = w->sent->body.ask_spawn.opts[0];
  }

  facing = w->incoming->body.reply_spawn.face;
  if (!map_valid_direction(facing)) {

    printf("Invalid direction %u\n", w->incoming->body.reply_spawn.face);
    facing = DIRECTION_NORTH;
  }

  player_spawn(&ctx->players[id], pos, facing);
  map_set_player(ctx->map, pos);
  ctx->players[id].los = map_line_of_sight(ctx->map, pos, facing);

  printf("Spawned player %d\n", id);

  return true;
}

static void resolve_moves(engine_t *ctx) {

  for (uint8_t i = 0; i < ctx->player_count; i++) {
    player_t *p = &ctx->players[i];

    /* Tagging a player makes it visible for an extra turn after
     * moving out of LoS
     */
    player_clear_tags(p);

    for (uint8_t j = 0; j < ctx->player_count; j++) {
      if (i == j) {
        continue;
      }

      if (map_opts_contains(p->los, ctx->players[j].position)) {
        player_tag(p, j);
      }
    }
  }

  for (uint8_t i = 0; i < ctx->player_count; i++) {
    bool ok = false;
    enum direction facing;
    player_t *p = &ctx->players[i];
    pos_t pos = p->position;

    struct waiting *w = &ctx->waiting[i];
    printf("Resolving move for player %u\n", i);

    if (spawn_reply(ctx, i)) {
      printf("  Player spawned, skipping move (w->type)\n");
      continue;
    }

    if (w->incoming == NULL || w->incoming->type != MESSAGE_REPLY_MOVE) {
      continue;
    }

    for (uint32_t j = 0; j < w->sent->body.ask_move.size; j++) {
      if (POS_EQ(w->incoming->body.reply_move.dst,
                 w->sent->body.ask_move.opts[j])) {
        ok = true;
        break;
      }
    }

    if (ok) {
      pos = w->incoming->body.reply_move.dst;
    }

    facing = w->incoming->body.reply_move.face;
    if (!map_valid_direction(facing)) {
      facing = DIRECTION_NORTH;
    }

    incident_t *inc = incident_new(ctx->incidents);
    inc->type = INCIDENT_PLAYER_MOVE;
    inc->player_origin = &ctx->players[i];
    inc->from = ctx->players[i].position;
    incident_new_target(inc, pos);

    player_position_update(ctx, i, pos, facing);

    /* Update spells if positioned on a portal */

    if (map_is_portal(ctx->map, p->position)) {
      portal_t *portal;
      incident_t *incident;

      portal = portals_get_at(ctx->portals, p->position);

      p->spells[portal->kind] = portal_get_spell(portal, ctx->turns + 3);
      if (p->spells[portal->kind] != NULL) {
        p->charges[portal->kind] = p->spells[portal->kind]->charges;
        incident = incident_new(ctx->incidents);
        incident->type = INCIDENT_PORTAL;
        incident->from = p->position;
      } else {
        p->charges[portal->kind] = 0;
      }
    }
  }
}

void add_portals_to_map_msg(engine_t *ctx, message_t *msg) {
  uint8_t count = 0;
  uint8_t num = portals_num(ctx->portals);

  msg->body.map.portals = malloc(num * sizeof(*msg->body.map.portals));

  for (uint32_t i = 0; i < num; i++) {
    portal_t *p;

    p = portals_get(ctx->portals, i);
    if (p == NULL) {
      continue;
    }

    msg->body.map.portals[count].pos = p->position;
    msg->body.map.portals[count].kind = p->kind;

    printf("Sending initial portal data (%d,%d) -> %u\n", p->position.x,
           p->position.y, p->kind);
    count++;
  }
  msg->body.map.num_portals = count;

  msg->body.map.num_players = ctx->player_count;
}

static message_t *build_player_update(engine_t *ctx, player_t *p) {
  map_opts_t *portals;
  uint8_t count = 0;
  message_t *msg;

  msg = message_player_update(ctx->tick);

  msg->body.player_update.player_id = p->id;
  msg->body.player_update.pos = p->position;
  msg->body.player_update.face = p->facing;
  msg->body.player_update.health = p->health;
  msg->body.player_update.kills = p->kills;
  msg->body.player_update.deaths = p->deaths;

  for (uint8_t i = 0; i < PORTAL_NONE; i++) {
    if (p->spells[i] != NULL) {
      msg->body.player_update.spells[i].id = p->spells[i]->id;
      msg->body.player_update.spells[i].charges = p->charges[i];

      printf("Updating playrt %u with spell id %u (%p)\n", p->id,
             p->spells[i]->id, p->spells[i]);
    } else {
      msg->body.player_update.spells[i].id = 0;
      msg->body.player_update.spells[i].charges = 0;
    }
  }

  if (p->los != NULL) {
    map_opts_export(p->los, &msg->body.player_update.los.opts,
                    &msg->body.player_update.los.size);
  } else {
    msg->body.player_update.los.opts = NULL;
    msg->body.player_update.los.size = 0;
  }

  msg->body.player_update.others =
      malloc(ctx->player_count * sizeof(*msg->body.player_update.others));
  count = 0;

  for (uint8_t i = 0; i < ctx->player_count; i++) {
    player_t *other;
    other = &ctx->players[i];
    if (p->health > 0 &&
        (other->id == p->id || !(player_is_tagged(p, i) ||
                                 map_opts_contains(p->los, other->position)))) {
      continue;
    }

    msg->body.player_update.others[count].player_id = other->id;
    msg->body.player_update.others[count].face = other->facing;
    msg->body.player_update.others[count].pos = other->position;
    msg->body.player_update.others[count].kills = other->kills;
    msg->body.player_update.others[count].deaths = other->deaths;
    msg->body.player_update.others[count].health = other->health;

    for (uint8_t j = 0; j < PORTAL_NONE; j++) {
      if (other->spells[j] != NULL) {
        msg->body.player_update.others[count].spells[j].id =
            other->spells[j]->id;
        msg->body.player_update.others[count].spells[j].charges =
            other->charges[j];
      } else {
        msg->body.player_update.others[count].spells[j].id = 0;
        msg->body.player_update.others[count].spells[j].charges = 0;
      }
    }

    count++;
  }
  msg->body.player_update.num_others = count;

  portals = map_portals(ctx->map, p->los);
  msg->body.player_update.portals =
      malloc(portals->size * sizeof(*msg->body.player_update.portals));
  count = 0;

  for (uint32_t i = 0; i < portals->size; i++) {
    portal_t *p;

    p = portals_get_at(ctx->portals, portals->data[i]);
    if (p == NULL) {
      continue;
    }

    msg->body.player_update.portals[count].pos = portals->data[i];
    msg->body.player_update.portals[count].kind = p->kind;
    msg->body.player_update.portals[count].spell =
        p->spell != NULL ? p->spell->id : 0;
    count++;
  }

  msg->body.player_update.num_portals = count;

  incident_add_to_message(ctx->incidents, p, msg);

  map_opts_free(portals);

  return msg;
}

static void update_players(engine_t *ctx) {
  for (uint8_t i = 0; i < ctx->player_count; i++) {
    message_t *msg;

    msg = build_player_update(ctx, &ctx->players[i]);
    clear_waiting(&ctx->waiting[i]);

    ctx->waiting[i].tick = ctx->tick;
    ctx->waiting[i].type = MESSAGE_REPLY_PLAYER_UPDATE;
    ctx->waiting[i].sent = msg;

    player_server_send_msg(&ctx->players[i], msg);
  }
  incident_ctx_clear(ctx->incidents);
}

static void ask_move(engine_t *ctx, uint8_t id) {
  message_t *msg;
  map_opts_t *moves;
  player_t *p;

  p = &ctx->players[id];

  moves = map_valid_moves(ctx->map, p->position, 15);

  msg = message_ask_move(ctx->tick, moves->size, moves->data);

  clear_waiting(&ctx->waiting[id]);

  ctx->waiting[id].tick = ctx->tick;
  ctx->waiting[id].type = MESSAGE_REPLY_MOVE;
  ctx->waiting[id].sent = msg;

  player_server_send_msg(p, msg);

  p->activated_spell = PORTAL_NONE;
  map_opts_free(moves);
}

static void ask_fight(engine_t *ctx) {
  for (uint8_t i = 0; i < ctx->player_count; i++) {
    message_t *msg;
    player_t *p = &ctx->players[i];

    msg = message_ask_fight(ctx->tick);

    printf("Ask figth for %u (%d,%d)\n", p->id, p->position.x, p->position.y);

    for (uint8_t j = 0; j < PORTAL_NONE; j++) {
      map_opts_t *in_range;
      const spell_t *spell;

      if (p->spells[j] == NULL || p->charges[j] == 0) {
        msg->body.ask_fight.spell_id[j] = 0;
        msg->body.ask_fight.spell_opts[j].size = 0;
        msg->body.ask_fight.spell_opts[j].opts = NULL;
        continue;
      }

      spell = p->spells[j];

      printf("Prepping spell for %u: %s, ragne %u\n", p->id, spell->name,
             spell->max_range);

      in_range = map_reduce_to_distance(ctx->map, p->position, p->los,
                                        spell->max_range);
      msg->body.ask_fight.spell_id[j] = spell->id;
      map_opts_export(in_range, &msg->body.ask_fight.spell_opts[j].opts,
                      &msg->body.ask_fight.spell_opts[j].size);
      map_opts_free(in_range);
    }

    clear_waiting(&ctx->waiting[i]);
    ctx->waiting[i].tick = ctx->tick;
    ctx->waiting[i].type = MESSAGE_REPLY_FIGHT;
    ctx->waiting[i].sent = msg;

    player_server_send_msg(&ctx->players[i], msg);
  }
}

static bool verify_spell(engine_t *ctx, const spell_t *spell, player_t *p,
                         pos_t target) {

  if (p->health <= 0) {
    return false;
  }

  printf("Verifying spell %s ( id %u), num ranges %u, max range %d\n",
         spell->name, spell->id, spell->num_ranges, spell->max_range);

  if (!map_within_distance(ctx->map, p->position, target,
                           spell->range[spell->num_ranges - 1].range)) {
    return false;
  }

  if (!map_has_los(ctx->map, p->position, target)) {
    return false;
  }

  return true;
}

static int8_t get_player_mod(player_t *p, enum spell_effect_types type) {
  int8_t total = 0;

  for (struct player_effect *eff = p->effects; eff != NULL; eff = eff->next) {
    if (eff->eff.type == type) {
      total += eff->eff.params.mod.value;
    }
  }
  return total;
}

static void apply_dmg_at(engine_t *ctx, incident_target_t *incident_target,
                         player_t *p, int8_t dmg_min, int8_t dmg_max,
                         pos_t target, bool selfdmg) {
  int8_t dmg;

  if (dmg_max <= 0) {
    printf("MAx dmg < 0\n");
    return;
  }

  for (uint8_t i = 0; i < ctx->player_count; i++) {
    player_t *other = &ctx->players[i];
    incident_effect_t *eff;

    if (!selfdmg && p->id == other->id) {
      continue;
    }

    if (!POS_EQ(target, other->position)) {
      continue;
    }

    if (other->health == 0 && other->injured_by == 0) {
      /* has been dead as before this round of damage */
      continue;
    }

    dmg = (rand() % (dmg_max - dmg_min)) + 1 + dmg_min;

    dmg += get_player_mod(other, SPELL_EFFECT_DAMAGE_MOD);

    if (dmg < 0) {
      dmg = 0;
    }

    eff = incident_new_effect(incident_target);
    eff->type = SPELL_EFFECT_DAMAGE;
    eff->victim = &ctx->players[i];
    eff->at = target;
    eff->data.dmg = dmg;
    printf("ADDING EFFECT FOR %d DAMAGE FOR PLAYER AT (%d,%d)\n", eff->data.dmg,
           eff->victim->position.x, eff->victim->position.y);

    if (dmg > 0) {
      other->health -= dmg;
      if (other->health < 0) {
        other->health = 0;
      }
      other->injured_by |= (1 << p->id);
    }
  }
}

static pos_t get_new_target(engine_t *ctx, pos_t from, pos_t to) {
  uint8_t steps;

  steps =
      ((to.x - from.x) * (to.x - from.x) + (to.y - from.y) * (to.y - from.y));

  steps = ((rand() % 100) * steps) / 100;

  if (steps < 3) {
    steps = 3;
  }

  if (steps > 30) {
    steps = 30;
  }

  for (uint8_t i = 0; i < steps; i++) {
    coord_t step;

    if (abs(to.x - from.x) > abs(to.y - from.y)) {
      step = to.y > from.y ? 1 : -1;

      to.y += step;
    } else {
      step = to.x > from.x ? 1 : -1;

      to.x += step;
    }
  }
  return map_ends_up_at(ctx->map, from, to);
}

static void apply_splash(engine_t *ctx, const struct spell_effect *eff,
                         pos_t target, incident_target_t *inc_targ,
                         player_t *caster) {

  map_opts_t *los = map_line_of_sight(ctx->map, target, DIRECTION_ANY);
  map_opts_delete(los, target); // Not hitting target again...
  map_opts_t *splashed = map_players(ctx->map, los);

  for (uint32_t s = 0; s < splashed->size; s++) {
    coord_t splash_min;
    coord_t splash_max;
    uint32_t dist_square =
        map_distance_squared(ctx->map, target, splashed->data[s]);

    coord_t dist = (coord_t)sqrt((double)dist_square);
    splash_min =
        eff->params.splash.dmg.min -
        ((dist / eff->params.splash.radius_step) * eff->params.splash.drop_of);
    splash_max =
        eff->params.splash.dmg.max -
        ((dist / eff->params.splash.radius_step) * eff->params.splash.drop_of);

    apply_dmg_at(ctx, inc_targ, caster, splash_min, splash_max,
                 splashed->data[s], true);
  }

  map_opts_free(los);
  map_opts_free(splashed);
}

static void apply_push_pull(engine_t *ctx, const struct spell_effect *eff,
                            pos_t target, incident_target_t *inc_targ,
                            player_t *caster) {
  pos_t new_pos;
  coord_t steps;
  incident_effect_t *inc_eff;

  if (POS_EQ(target, caster->position)) {
    return;
  }

  for (uint8_t i = 0; i < ctx->player_count; i++) {
    player_t *candidate;
    candidate = &ctx->players[i];
    if (!POS_EQ(candidate->position, target)) {
      continue;
    }

    if (eff->params.move.max > eff->params.move.min) {
      steps = eff->params.move.min +
              (rand() % (eff->params.move.max - eff->params.move.min));
    } else {
      steps = eff->params.move.max;
    }
    if (eff->type == SPELL_EFFECT_PULL) {
      new_pos = map_pull(ctx->map, caster->position, target, steps);
    } else {
      new_pos = map_push(ctx->map, caster->position, target, steps);
    }
    inc_eff = incident_new_effect(inc_targ);
    inc_eff->victim = candidate;
    inc_eff->at = candidate->position;
    inc_eff->data.new_pos = new_pos;
    inc_eff->type = eff->type;
    candidate->position = new_pos;
  }
}

static void apply_push_random(engine_t *ctx, const struct spell_effect *eff,
                              pos_t target, incident_target_t *inc_targ,
                              player_t *caster) {
  pos_t new_pos;
  map_opts_t *outer;
  map_opts_t *inner;
  incident_effect_t *inc_eff;

  coord_t max = eff->params.move.max;
  coord_t min = eff->params.move.max;

  if (max <= min) {
    if (min > 0) {
      min--;
    } else {
      max++;
    }
  }

  outer = map_valid_moves(ctx->map, target, max);
  inner = map_valid_moves(ctx->map, target, min);
  map_opts_delete_list(outer, inner);

  if (outer->size == 0) {
    return;
  }

  for (uint8_t i = 0; i < ctx->player_count; i++) {
    player_t *candidate;

    candidate = &ctx->players[i];
    if (!POS_EQ(candidate->position, target)) {
      continue;
    }

    map_opts_shuffle(outer);
    new_pos = outer->data[i];
    inc_eff = incident_new_effect(inc_targ);
    inc_eff->victim = candidate;
    inc_eff->at = candidate->position;
    inc_eff->data.new_pos = new_pos;
    inc_eff->type = SPELL_EFFECT_PUSH_RANDOM;
    candidate->position = new_pos;
  }
  map_opts_free(outer);
  map_opts_free(inner);
}

static void apply_heal(engine_t *ctx, const struct spell_effect *eff,
                       pos_t target, incident_target_t *inc_targ,
                       player_t *caster) {
  incident_effect_t *inc_eff;

  for (uint8_t i = 0; i < ctx->player_count; i++) {
    player_t *candidate;
    int8_t amount;

    candidate = &ctx->players[i];
    if (!POS_EQ(candidate->position, target)) {
      continue;
    }

    amount = eff->params.heal.min +
             (rand() % (eff->params.heal.max - eff->params.heal.min));

    inc_eff = incident_new_effect(inc_targ);
    inc_eff->victim = candidate;
    inc_eff->at = candidate->position;
    inc_eff->data.dmg = amount;
    inc_eff->type = SPELL_EFFECT_HEAL;
    candidate->health += amount;
    if (candidate->health > 100) {
      candidate->health = 100;
    }
  }
}
static void apply_poison(engine_t *ctx, const struct spell_effect *eff,
                         pos_t target, incident_target_t *inc_targ,
                         player_t *caster) {
  incident_effect_t *inc_eff;

  for (uint8_t i = 0; i < ctx->player_count; i++) {
    player_t *candidate;

    candidate = &ctx->players[i];
    if (!POS_EQ(candidate->position, target)) {
      continue;
    }

    inc_eff = incident_new_effect(inc_targ);
    inc_eff->victim = candidate;
    inc_eff->at = candidate->position;
    inc_eff->data.duration = eff->params.poison.duration;
    inc_eff->type = SPELL_EFFECT_POISON;

    player_add_effect(candidate, *eff, inc_eff->data,
                      eff->params.poison.duration, caster);
  }
}

static void apply_poison_effects(engine_t *ctx) {
  for (uint8_t i = 0; i < ctx->player_count; i++) {
    player_t *candidate;
    int8_t dmg;

    candidate = &ctx->players[i];

    for (struct player_effect *e = candidate->effects; e != NULL; e = e->next) {
      incident_effect_t *eff;
      incident_target_t *inc_targ;
      incident_t *inc;

      if (e->eff.type != SPELL_EFFECT_POISON) {
        continue;
      }

      if (candidate->health <= 0 && candidate->injured_by == 0) {
        continue;
      }
      dmg = (rand() % (e->eff.params.poison.max - e->eff.params.poison.min)) +
            1 + e->eff.params.poison.min;

      inc = incident_new(ctx->incidents);
      inc->type = INCIDENT_DELAYED_EFFECT;
      inc_targ = incident_new_target(inc, candidate->position);
      eff = incident_new_effect(inc_targ);
      eff->type = SPELL_EFFECT_DAMAGE;
      eff->victim = &ctx->players[i];
      eff->at = candidate->position;
      eff->data.dmg = dmg;

      if (dmg > 0) {
        candidate->health -= dmg;
        candidate->injured_by |= (1 << e->caster->id);
        if (candidate->health < 0) {
          candidate->health = 0;
        }
      }
    }
  }
  resolve_deaths(ctx);
}

static void apply_mod(engine_t *ctx, const struct spell_effect *eff,
                      pos_t target, incident_target_t *inc_targ,
                      player_t *caster) {
  incident_effect_t *inc_eff;

  for (uint8_t i = 0; i < ctx->player_count; i++) {
    player_t *candidate;

    candidate = &ctx->players[i];
    if (!POS_EQ(candidate->position, target)) {
      continue;
    }

    inc_eff = incident_new_effect(inc_targ);
    inc_eff->victim = candidate;
    inc_eff->at = candidate->position;
    inc_eff->data.duration = eff->params.mod.duration;
    inc_eff->type = eff->type;

    player_add_effect(candidate, *eff, inc_eff->data, eff->params.mod.duration,
                      caster);
  }
}

static void apply_effects(engine_t *ctx, const spell_t *spell, pos_t target,
                          incident_target_t *inc_target, player_t *caster) {

  for (uint8_t i = 0; i < spell->num_effects; i++) {
    switch (spell->effect[i].type) {
    case SPELL_EFFECT_SPLASH:
      apply_splash(ctx, &spell->effect[i], target, inc_target, caster);
      break;
    case SPELL_EFFECT_DAMAGE:
      /* Already handled */
      break;
    case SPELL_EFFECT_PUSH:
    case SPELL_EFFECT_PULL:
      apply_push_pull(ctx, &spell->effect[i], target, inc_target, caster);
      break;
    case SPELL_EFFECT_PUSH_RANDOM:
      apply_push_random(ctx, &spell->effect[i], target, inc_target, caster);
      break;
    case SPELL_EFFECT_HEAL:
      apply_heal(ctx, &spell->effect[i], target, inc_target, caster);
      break;
    case SPELL_EFFECT_POISON:
      apply_poison(ctx, &spell->effect[i], target, inc_target, caster);
      break;
    case SPELL_EFFECT_DAMAGE_MOD:
    case SPELL_EFFECT_HIT_MOD:
    case SPELL_EFFECT_BE_HIT_MOD:
      apply_mod(ctx, &spell->effect[i], target, inc_target, caster);
      break;
    case SPELL_EFFECT_OBSCURE:
      /* TODO */
      break;
    }
  }
}

static void apply_spell(engine_t *ctx, const spell_t *spell, player_t *p,
                        pos_t target) {

  player_t *other;
  int8_t dmg_min = 0;
  int8_t dmg_max = 0;
  int8_t hit = 0;
  coord_t dist_square;
  incident_t *incident = incident_new(ctx->incidents);

  printf("Applying spell with id %u, %u num_ranges, max_range %d\n", spell->id,
         spell->num_ranges, spell->max_range);

  dist_square = map_distance_squared(ctx->map, p->position, target);

  spell_get_stats(spell, dist_square, &hit, &dmg_min, &dmg_max);

  p->activated_spell = spell->kind;
  p->charges[spell->kind]--;

  incident->type = INCIDENT_SPELL;
  incident->player_origin = p;
  incident->from = p->position;
  incident->spell = spell;

  /** TODO: Move to spell effects? */
  for (uint8_t i = 0; i < ctx->player_count; i++) {
    other = &ctx->players[i];
    if (other->id != p->id && POS_EQ(other->position, target)) {
      hit += get_player_mod(other, SPELL_EFFECT_BE_HIT_MOD);
    }
  }

  hit += get_player_mod(other, SPELL_EFFECT_HIT_MOD);

  for (int8_t i = 0; i < spell->burst; i++) {
    incident_target_t *target_incident;
    pos_t burst_target = target;

    if (hit > 0 && rand() % 100 < hit) {
      /* rand() starts at 0, so < gives fair % */
      printf("Spell hit (%d)\n", hit);
      target_incident = incident_new_target(incident, target);
      apply_dmg_at(ctx, target_incident, p, dmg_min, dmg_max, target, false);
    } else {
      printf("Spell miss (%d)\n", hit);
      switch (spell->miss) {
      case SPELL_MISS_LOS: {
        int8_t miss_dmg_min = 0;
        int8_t miss_dmg_max = 0;

        burst_target = get_new_target(ctx, p->position, target);
        target_incident = incident_new_target(incident, burst_target);
        dist_square = map_distance_squared(ctx->map, p->position, burst_target);
        spell_get_stats(spell, dist_square, NULL, &miss_dmg_min, &miss_dmg_max);

        apply_dmg_at(ctx, target_incident, p, dmg_min, dmg_max, burst_target,
                     true);

      } break;
      case SPELL_MISS_BOUNCE: {
        map_opts_t *opts;

        opts = map_valid_moves(ctx->map, target, spell->bounce_max);
        map_opts_delete(opts, target);
        map_opts_shuffle(opts);
        burst_target = opts->data[i];
        target_incident = incident_new_target(incident, burst_target);
        apply_dmg_at(ctx, target_incident, p, dmg_min, dmg_max, burst_target,
                     true);

        map_opts_free(opts);
      } break;
      case SPELL_MISS_INTERRUPT:
        return;
      case SPELL_MISS_NONE:
        continue;
        break;
      }
    }

    apply_effects(ctx, spell, burst_target, target_incident, p);
  }
}

static void resolve_fight(engine_t *ctx) {

  while (true) {
    player_t *acting = NULL;
    const spell_t *candidate = NULL;

    /* First find the highest remaining speed */
    for (uint8_t i = 0; i < ctx->player_count; i++) {
      const spell_t *check;

      if (ctx->waiting[i].incoming == NULL) {
        continue;
      }

      if (ctx->waiting[i].incoming->body.reply_fight.spell_id == 0) {
        clear_waiting(&ctx->waiting[i]);
        continue;
      }

      check =
          spell_get_by_id(ctx->waiting[i].incoming->body.reply_fight.spell_id);

      if (check == NULL) {
        continue;
      }

      if (candidate == NULL || check->speed > candidate->speed) {
        candidate = check;
        acting = &ctx->players[i];
      }
    }

    if (acting == NULL) {
      /* No unprocessed attacks - done */
      break;
    }

    /* Remove all attacks at current speed that are now invalid */
    for (uint8_t i = 0; i < ctx->player_count; i++) {
      const spell_t *check;

      if (ctx->waiting[i].incoming == NULL) {
        continue;
      }

      check =
          spell_get_by_id(ctx->waiting[i].incoming->body.reply_fight.spell_id);

      if (check == NULL || check->speed != candidate->speed) {
        continue;
      }

      if (!verify_spell(ctx, check, &ctx->players[i],
                        ctx->waiting[i].incoming->body.reply_fight.target)) {
        clear_waiting(&ctx->waiting[i]);
      }
    }

    /* Apply the remaining attacks at the current speed */

    for (uint8_t i = 0; i < ctx->player_count; i++) {
      const spell_t *check;
      if (ctx->waiting[i].incoming == NULL) {
        continue;
      }

      check =
          spell_get_by_id(ctx->waiting[i].incoming->body.reply_fight.spell_id);

      if (check == NULL || check->speed != candidate->speed) {
        continue;
      }

      apply_spell(ctx, check, &ctx->players[i],
                  ctx->waiting[i].incoming->body.reply_fight.target);

      clear_waiting(&ctx->waiting[i]);
    }

    /* All damage applied, check if anybody died */
    resolve_deaths(ctx);
  }
}
void engine_tick(engine_t *ctx) {
  message_t *msg;

  ctx->tick++;
  switch (ctx->state) {
  case STATE_STARTING:
    players_wait(ctx, MESSAGE_REPLY_READY);
    msg = message_ask_ready(ctx->tick);
    send_all(ctx, msg);
    message_unref(msg);

    ctx->state = STATE_WAIT_READY;
    break;

  case STATE_WAIT_READY:
    if (players_ready(ctx)) {

      players_wait(ctx, MESSAGE_REPLY_MAP);
      msg = map_to_message(ctx->map, ctx->tick);
      add_portals_to_map_msg(ctx, msg);
      send_all(ctx, msg);
      message_unref(msg);
      ctx->state = STATE_WAIT_MAP;
    }
    break;

  case STATE_WAIT_MAP:
    if (players_ready(ctx)) {
      ctx->init_spawn_active = 0;
      for (uint8_t i = 0; i < ctx->player_count; i++) {
        clear_waiting(&ctx->waiting[i]);
      }
      ask_spawn(ctx, ctx->init_spawn_active);
      ctx->state = STATE_WAIT_INIT_SPAWN;
    }
    break;

  case STATE_WAIT_INIT_SPAWN:
    if (players_ready(ctx)) {
      spawn_reply(ctx, ctx->init_spawn_active);
      clear_waiting(&ctx->waiting[ctx->init_spawn_active]);
      printf("Got spawn reply from %d\n", ctx->init_spawn_active);
      ctx->init_spawn_active++;
      if (ctx->init_spawn_active < ctx->player_count) {
        ask_spawn(ctx, ctx->init_spawn_active);
      } else {
        update_players(ctx);
        ctx->state = STATE_WAIT_PLAYER_UPDATE_ACK;
      }
    }
    break;

  case STATE_WAIT_PLAYER_UPDATE_ACK:
    if (players_ready(ctx)) {
      ctx->state = STATE_ASK_MOVE;
    }
    break;

  case STATE_ASK_MOVE:
    for (uint8_t i = 0; i < ctx->player_count; i++) {
      if (ctx->players[i].health > 0) {
        printf("asking move from %d", i);
        ask_move(ctx, i);
      } else {
        printf("asking spawn from %d", i);
        ask_spawn(ctx, i);
      }
    }
    ctx->state = STATE_WAIT_MOVE;
    break;

  case STATE_WAIT_MOVE:
    if (players_ready(ctx)) {
      resolve_moves(ctx);
      update_players(ctx);
      ctx->state = STATE_WAIT_MOVE_PLAYER_UPDATE_ACK;
    }
    break;

  case STATE_WAIT_MOVE_PLAYER_UPDATE_ACK:
    if (players_ready(ctx)) {
      ctx->state = STATE_ASK_FIGHT;
    }

    break;

  case STATE_ASK_FIGHT:
    ask_fight(ctx);
    ctx->state = STATE_WAIT_FIGHT;
    break;

  case STATE_WAIT_FIGHT:
    if (players_ready(ctx)) {
      resolve_fight(ctx);
      apply_poison_effects(ctx);
      for (uint8_t i = 0; i < ctx->player_count; i++) {
        player_time_effects(&ctx->players[i]);
      }
      update_players(ctx);
      ctx->turns++;
      portals_activate(ctx->portals, ctx->turns);
      ctx->state = STATE_WAIT_FIGHT_PLAYER_UPDATE_ACK;
    }
    break;

  case STATE_WAIT_FIGHT_PLAYER_UPDATE_ACK:
    if (players_ready(ctx)) {
      ctx->state = STATE_ASK_MOVE;
    }

    break;
  }
}

bool engine_add_player(engine_t *ctx, player_send_msg_func_t send,
                       void *send_ctx, player_get_msg_func_t get,
                       void *get_ctx) {
  for (uint8_t i = 0; i < ctx->player_count; i++) {
    if (ctx->players[i].brain.server_send == NULL) {
      ctx->players[i].brain.server_send = send;
      ctx->players[i].brain.server_send_user_data = send_ctx;
      ctx->players[i].brain.server_get = get;
      ctx->players[i].brain.server_get_user_data = get_ctx;

      return true;
    }
  }

  return false;
}
