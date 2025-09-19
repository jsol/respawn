#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "map_opts.h"
#include "message.h"

struct msg_box {
  message_t msg;
  int8_t refcount;
};

static message_t *new_msg(uint32_t tick, enum message_type type) {
  struct msg_box *box;

  box = malloc(sizeof(*box));
  box->refcount = 1;

  box->msg.tick = tick;
  box->msg.type = type;

  return &box->msg;
}

message_t *message_map(uint32_t tick) {
  message_t *msg;

  msg = new_msg(tick, MESSAGE_MAP);

  return msg;
}

message_t *message_reply_map(uint32_t tick) {
  message_t *msg;

  msg = new_msg(tick, MESSAGE_REPLY_MAP);

  return msg;
}

message_t *message_reply_ready(uint32_t tick) {
  message_t *msg;

  msg = new_msg(tick, MESSAGE_REPLY_READY);

  return msg;
}

message_t *message_reply_spawn(uint32_t tick, pos_t pos, uint8_t facing) {
  message_t *msg;

  msg = new_msg(tick, MESSAGE_REPLY_SPAWN);
  msg->body.reply_spawn.dst = pos;
  msg->body.reply_spawn.face = facing;

  return msg;
}

message_t *message_reply_move(uint32_t tick, pos_t pos, uint8_t facing) {
  message_t *msg;

  msg = new_msg(tick, MESSAGE_REPLY_MOVE);
  msg->body.reply_move.dst = pos;
  msg->body.reply_move.face = facing;

  return msg;
}

message_t *message_reply_player_update(uint32_t tick) {
  message_t *msg;

  msg = new_msg(tick, MESSAGE_REPLY_PLAYER_UPDATE);

  return msg;
}
message_t *message_reply_fight(uint32_t tick, uint8_t spell_id, pos_t target) {
  message_t *msg;

  msg = new_msg(tick, MESSAGE_REPLY_FIGHT);
  msg->body.reply_fight.target = target;
  msg->body.reply_fight.spell_id = spell_id;

  return msg;
}

message_t *message_ask_ready(uint32_t tick) {
  message_t *msg;

  msg = new_msg(tick, MESSAGE_ASK_READY);

  return msg;
}

message_t *message_player_update(uint32_t tick) {
  message_t *msg;

  msg = new_msg(tick, MESSAGE_PLAYER_UPDATE);

  msg->body.player_update.events = NULL;
  msg->body.player_update.num_events = 0;

  return msg;
}

message_t *message_ask_spawn(uint32_t tick, uint8_t player_id, uint32_t size,
                             pos_t *data) {
  message_t *msg;

  msg = new_msg(tick, MESSAGE_ASK_SPAWN);
  msg->body.ask_spawn.player_id = player_id;
  msg->body.ask_spawn.size = size;
  msg->body.ask_spawn.opts = malloc(size * sizeof(*data));
  memcpy(msg->body.ask_spawn.opts, data, size * sizeof(*data));

  return msg;
}
message_t *message_ask_move(uint32_t tick, uint32_t size, pos_t *data) {
  message_t *msg;

  msg = new_msg(tick, MESSAGE_ASK_MOVE);
  msg->body.ask_move.size = size;
  msg->body.ask_move.opts = malloc(size * sizeof(*data));
  memcpy(msg->body.ask_spawn.opts, data, size * sizeof(*data));

  return msg;
}

message_t *message_ask_fight(uint32_t tick) {
  message_t *msg;

  msg = new_msg(tick, MESSAGE_ASK_FIGHT);

  return msg;
}

message_t *message_ref(message_t *msg) {
  struct msg_box *box;

  if (msg == NULL) {
    return msg;
  }

  box = (struct msg_box *)msg;
  box->refcount++;

  return msg;
}

void message_unref(message_t *msg) {
  struct msg_box *box;

  if (msg == NULL) {
    return;
  }

  box = (struct msg_box *)msg;
  box->refcount--;

  if (box->refcount > 0) {
    return;
  }

  switch (msg->type) {
  case MESSAGE_MAP:
    free(msg->body.map.data);
    free(msg->body.map.portals);
    break;
  case MESSAGE_ASK_SPAWN:
    free(msg->body.ask_spawn.opts);
    break;

  case MESSAGE_ASK_MOVE:
    free(msg->body.ask_move.opts);
    break;

  case MESSAGE_ASK_FIGHT:
    for (uint8_t i = 0; i < PORTAL_NONE; i++) {
      free(msg->body.ask_fight.spell_opts[i].opts);
    }
    break;

  case MESSAGE_PLAYER_UPDATE:
    free(msg->body.player_update.los.opts);
    free(msg->body.player_update.others);
    free(msg->body.player_update.portals);
    for (uint8_t i = 0; i < msg->body.player_update.num_events; i++) {
      for (uint8_t j = 0; j < msg->body.player_update.events[i].num_targets;
           j++) {
        free(msg->body.player_update.events[i].targets[j].effects);
      }
      free(msg->body.player_update.events[i].targets);
    }
    free(msg->body.player_update.events);
    break;
  default:
    break;
  }

  free(box);
}
