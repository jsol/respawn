#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "message.h"
#include "portals.h"
#include "spell.h"

struct portals_ctx {
  uint32_t size;
  uint32_t capacity;
  portal_t *data;
};

portals_ctx_t *portals_new(uint32_t capacity) {
  portals_ctx_t *ctx;

  ctx = malloc(sizeof(*ctx));
  ctx->data = malloc(sizeof(*ctx->data) * capacity);
  ctx->size = 0;
  ctx->capacity = capacity;

  spell_init();

  return ctx;
}

portals_ctx_t *portals_new_from_message(message_t *msg) {
  portals_ctx_t *ctx;

  ctx = malloc(sizeof(*ctx));
  ctx->data = malloc(sizeof(*ctx->data) * msg->body.map.num_portals);
  ctx->size = 0;
  ctx->capacity = msg->body.map.num_portals;

  spell_init();

  for (uint8_t i = 0; i < msg->body.map.num_portals; i++) {
    portal_t *portal;

    portal = &ctx->data[ctx->size];

    portal->position = msg->body.map.portals[i].pos;

    portal->kind = msg->body.map.portals[i].kind;
    portal->spell = NULL;
    portal->activate = UINT32_MAX;

    ctx->size++;
  }

  return ctx;
}

void portals_update(portals_ctx_t *ctx, message_t *msg) {
  for (uint8_t i = 0; i < msg->body.player_update.num_portals; i++) {

    for (uint8_t j = 0; j < ctx->size; j++) {
      portal_t *portal;

      portal = &ctx->data[j];

      if (!POS_EQ(portal->position, msg->body.player_update.portals[i].pos)) {
        continue;
      }

      portal->kind = msg->body.player_update.portals[i].kind;
      portal->spell = spell_get_by_id(msg->body.player_update.portals[i].spell);
    }
  }
}

uint8_t portals_num(portals_ctx_t *ctx) {
  return ctx->size;
}

portal_t *portals_get(portals_ctx_t *ctx, uint8_t id) {
  if (id >= ctx->size) {
    return NULL;
  }
  return &ctx->data[id];
}

void portals_add_kind(portals_ctx_t *ctx, enum portal_type kind, pos_t pos) {
  portal_t *portal;

  if (ctx->size == ctx->capacity) {
    ctx->capacity = ctx->capacity * 2;
    // TODO: Fix NULL return
    ctx->data = realloc(ctx->data, sizeof(*ctx->data) * ctx->capacity);
  }

  portal = &ctx->data[ctx->size];

  portal->position = pos;
  portal->kind = kind;
  portal->spell = spell_get_random(kind);
  portal->activate = UINT32_MAX;

  ctx->size++;
}

portal_t *portals_get_at(portals_ctx_t *ctx, pos_t pos) {
  for (uint32_t i = 0; i < ctx->size; i++) {
    if (POS_EQ(pos, ctx->data[i].position)) {
      return &ctx->data[i];
    }
  }

  return NULL;
}

void portals_activate(portals_ctx_t *ctx, uint32_t active) {
  for (uint32_t i = 0; i < ctx->size; i++) {
    if (ctx->data[i].activate <= active) {
      ctx->data[i].spell = spell_get_random(ctx->data[i].kind);
      ctx->data[i].activate = UINT32_MAX;
    }

    if (ctx->data[i].activate != UINT32_MAX) {
      /* Ensure that portals waiting for activation does not provide spells */
      ctx->data[i].spell = NULL;
    }
  }
}

const spell_t *portal_get_spell(portal_t *ctx, uint32_t ready_again) {
  const spell_t *ret = ctx->spell;

  ctx->activate = ready_again;

  return ret;
}
