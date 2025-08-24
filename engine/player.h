#pragma once
#include <stdbool.h>
#include <stdint.h>

#include "common.h"
#include "map_opts.h"
#include "message.h"
#include "spell.h"

typedef struct player_ctx player_t;

typedef void (*player_send_msg_func_t)(void *user_data, message_t *msg);
typedef message_t *(*player_get_msg_func_t)(void *user_data);
typedef void (*player_new_msg_func_t)(message_t *message, void *user_data);

struct player_ctx {
  uint32_t id;
  enum direction facing;
  pos_t position;
  const spell_t *spells[PORTAL_NONE];
  int8_t charges[PORTAL_NONE];
  int8_t health;
  int8_t kills;
  int8_t deaths;

  map_opts_t *los;

  enum portal_type activated_spell;

  uint32_t injured_by;
  uint32_t tagged;

  int8_t to_be_hit_mod;
  int8_t to_hit_mod;
  int8_t to_dmg_mod;

  struct {
    player_send_msg_func_t client_send;
    void *client_send_user_data;
    player_send_msg_func_t server_send;
    void *server_send_user_data;
    player_get_msg_func_t client_get;
    void *client_get_user_data;
    player_get_msg_func_t server_get;
    void *server_get_user_data;
    player_new_msg_func_t client_hook;
    void *client_hook_user_data;
  } brain;
};

player_t *player_new(uint32_t id);
player_t *player_create(uint32_t num);

/* Note that @param firs is a pointer to the memory block from players_create */
void player_batch_update(player_t *first, uint32_t num_players, message_t *msg);

void player_tag(player_t *ctx, uint8_t other_id);
bool player_is_tagged(player_t *ctx, uint8_t other_id);
void player_clear_tags(player_t *ctx);

void player_spawn(player_t *ctx, pos_t pos, enum direction facing);
void player_killed(player_t *ctx);

void player_client_send_msg(player_t *ctx, message_t *msg);
message_t *player_client_get_msg(player_t *ctx);

void player_server_send_msg(player_t *ctx, message_t *msg);
message_t *player_server_get_msg(player_t *ctx);
