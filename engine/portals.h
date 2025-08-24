#pragma once
#include <stdint.h>

#include "common.h"
#include "message.h"
#include "spell.h"

typedef struct portals_ctx portals_ctx_t;
typedef struct {
  enum portal_type kind;
  pos_t position;
  uint32_t activate;
  const spell_t *spell;
} portal_t;

portals_ctx_t *portals_new(uint32_t capacity);
portals_ctx_t *portals_new_from_message(message_t *msg);
void portals_update(portals_ctx_t *ctx, message_t *msg);

void portals_add_kind(portals_ctx_t *ctx, enum portal_type kind, pos_t pos);

portal_t *portals_get_at(portals_ctx_t *ctx, pos_t pos);

void portals_activate(portals_ctx_t *ctx, uint32_t active);

const spell_t *portal_get_spell(portal_t *ctx, uint32_t ready_again);
