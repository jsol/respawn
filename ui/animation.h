#pragma once
#include <raylib.h>
#include <stdbool.h>
#include <stdint.h>

enum animation_type {
  ANIMATION_PORTAL_USE = 0,
  ANIMATION_SPELL_AURA,
  ANIMATION_SPELL_INVOCATION,
  ANIMATION_DAMAGE,
  ANIMATION_PLAYER_DEATH,
  ANIMATION_PLAYER_MOVED,
  ANIMATION_PLAYER_MOVE_ONE,
  ANIMATION_PLAYER_MOVE_TWO,
  ANIMATION_GROWING_BALL,
  ANIMATION_LAST
};

typedef struct animation_ctx animation_t;

animation_t *animation_new(uint32_t screen_fps, uint8_t player_count);
void animation_next_slot(animation_t *ctx);
void animation_prev_slot(animation_t *ctx);

void animation_queue(animation_t *ctx, enum animation_type type, Vector2 start,
                     float rotation, Color tint);

void animation_queue_moving(animation_t *ctx, enum animation_type type,
                            Vector2 start, Vector2 stop, float rotation,
                            Color tint);

void animation_queue_damage(animation_t *ctx, uint8_t dmg, Vector2 start);
void animation_queue_player(animation_t *ctx, enum animation_type type,
                            uint8_t id, Vector2 start, Vector2 stop,
                            float target_angle);

void animation_tick(animation_t *ctx);

bool animation_active(animation_t *ctx);
void animation_clean(animation_t *ctx);

void animation_set_player(animation_t *ctx, uint8_t player, float x, float y,
                          float rotation);
void animation_draw_player(animation_t *ctx, uint8_t id, float x, float y);
void animation_draw_players(animation_t *ctx);
