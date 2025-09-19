#include <math.h>
#include <raylib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "animation.h"
#include "asset.h"

#define ASSET_PATH "/home/jens/git/respawn/assets/"

struct source {
  enum animation_type id;
  Texture2D texture;
  uint8_t frames;
  int32_t tile_height;
  int32_t tile_width;
};

struct player {
  Rectangle dest;
  Rectangle source;
  float angle;
  Color color;
  bool animated;
};

struct animation {
  uint8_t step;
  struct source *src;
  char text[9];
  Vector2 start;
  Vector2 diff; /* move pixels each step */

  float rotation;
  float rotation_diff;
  Color tint;
  struct player *player;
};

struct animation_slot {
  struct animation *animations;
  int32_t num_animations;
  int32_t cap_animations;
  bool done;
};

struct animation_ctx {
  struct source src[ANIMATION_LAST];
  uint32_t fps;
  bool draw;

  Texture2D player_image;
  struct player *players;
  uint8_t player_count;

  struct animation_slot *slots;
  int32_t num_slots;
  int32_t cap_slots;
  int32_t add_slot;
};

static void load_texture(enum asset asset, int32_t width,
                         struct source *target) {
  Image img;

  img = asset_get(asset);

  //  img = LoadImage(file);
  target->texture = LoadTextureFromImage(img);
  target->frames = img.width / width;
  target->tile_width = width;
  target->tile_height = img.height;

  //  printf("ASSET LOADED %s (%dx%d )\n", file, img.width, img.height);

  //  if (file != NULL ) {UnloadImage(img);}
}

static Color player_color(uint8_t id) {
  switch (id) {
  case 0:
    return CLITERAL(Color){47, 79, 79, 255};
  case 1:
    return CLITERAL(Color){0, 100, 0, 255};
  case 2:
    return CLITERAL(Color){189, 183, 107, 255};
  case 3:
    return CLITERAL(Color){0, 0, 128, 255};
  case 4:
    return CLITERAL(Color){255, 0, 0, 255};
  case 5:
    return CLITERAL(Color){255, 165, 0, 255};
  case 6:
    return CLITERAL(Color){255, 255, 0, 255};
  case 7:
    return CLITERAL(Color){199, 21, 133, 255};
  case 8:
    return CLITERAL(Color){0, 255, 0, 255};
  case 9:
    return CLITERAL(Color){0, 250, 154, 255};
  case 10:
    return CLITERAL(Color){0, 255, 255, 255};
  case 11:
    return CLITERAL(Color){0, 0, 255, 255};
  case 12:
    return CLITERAL(Color){216, 191, 216, 255};
  case 13:
    return CLITERAL(Color){255, 0, 255, 255};
  case 14:
    return CLITERAL(Color){30, 144, 255, 255};
  case 15:
    return CLITERAL(Color){250, 128, 114, 255};
  default:
    return CLITERAL(Color){rand() % 255, rand() % 255, rand() % 255, 255};
  }
}

animation_t *animation_new(uint32_t screen_fps, uint8_t player_count) {
  Image img;
  animation_t *ctx = NULL;

  ctx = malloc(sizeof(*ctx));

  printf("NEW ANIMATION %d, %d\n", screen_fps, player_count);
  ctx->fps = screen_fps;
  for (uint8_t i = 0; i < ANIMATION_LAST; i++) {
    ctx->src[i].id = i;
  }

  ctx->src[ANIMATION_DAMAGE].frames = 30;
  load_texture(ASSET_BLINK_FIRE, 20, &ctx->src[ANIMATION_PORTAL_USE]);
  load_texture(ASSET_CAST, 40, &ctx->src[ANIMATION_SPELL_AURA]);
  load_texture(ASSET_GROWING_BALL, 20, &ctx->src[ANIMATION_GROWING_BALL]);
  load_texture(ASSET_DYING, 30, &ctx->src[ANIMATION_PLAYER_DEATH]);
  load_texture(ASSET_SOURCERER2, 20, &ctx->src[ANIMATION_PLAYER_MOVED]);
  load_texture(ASSET_MOVE1, 30, &ctx->src[ANIMATION_PLAYER_MOVE_ONE]);
  load_texture(ASSET_MOVE2, 30, &ctx->src[ANIMATION_PLAYER_MOVE_TWO]);
  /*
    load_texture(ASSET_PATH "blink_fire.png", 20,
                 &ctx->src[ANIMATION_PORTAL_USE]);
    load_texture(ASSET_PATH "cast.png", 40, &ctx->src[ANIMATION_SPELL_AURA]);
    load_texture(ASSET_PATH "growing_ball.png", 20,
                 &ctx->src[ANIMATION_GROWING_BALL]);
    load_texture(ASSET_PATH "dying.png", 30, &ctx->src[ANIMATION_PLAYER_DEATH]);
    load_texture(ASSET_PATH "sourcerer2.png", 20,
                 &ctx->src[ANIMATION_PLAYER_MOVED]);
    load_texture(ASSET_PATH "move1.png", 30,
                 &ctx->src[ANIMATION_PLAYER_MOVE_ONE]);
    load_texture(ASSET_PATH "move2.png", 30,
                 &ctx->src[ANIMATION_PLAYER_MOVE_TWO]);
  */
  ctx->src[ANIMATION_PLAYER_MOVED].frames = 15; /* Override */
  ctx->player_count = player_count;

  ctx->players = calloc(player_count, sizeof(*ctx->players));

  //img = LoadImage(ASSET_PATH "sourcerer2.png");
  img = asset_get(ASSET_SOURCERER2);
  for (uint8_t i = 0; i < player_count; i++) {
    ctx->players[i].source.width = img.width;
    ctx->players[i].source.height = img.height;
    ctx->players[i].dest.width = img.width;
    ctx->players[i].dest.height = img.height;
    ctx->players[i].dest.x = -1;
    ctx->players[i].dest.y = -1;
    ctx->players[i].color = player_color(i);
  }

  ctx->player_image = LoadTextureFromImage(img);
  printf("Loaded player image\n");
//  UnloadImage(img);

  ctx->cap_slots = 30;
  ctx->num_slots = 0;
  ctx->add_slot = -1;
  ctx->slots = calloc(ctx->cap_slots, sizeof(*ctx->slots));

  animation_next_slot(ctx);

  return ctx;
}

struct animation *get_animation(animation_t *ctx) {

  struct animation_slot *slot = &ctx->slots[ctx->add_slot];
  struct animation *ani = NULL;

  if (slot->num_animations >= slot->cap_animations) {
    slot->cap_animations = slot->cap_animations * 2;
    slot->animations = realloc(slot->animations, slot->cap_animations *
                                                     sizeof(*slot->animations));
  }

  ani = &slot->animations[slot->num_animations];
  slot->num_animations++;

  memset(ani, 0, sizeof(*ani));

  return ani;
}

void animation_set_player(animation_t *ctx, uint8_t player, float x, float y,
                          float rotation) {
  ctx->players[player].dest.x = x;
  ctx->players[player].dest.y = y;
  ctx->players[player].angle = rotation;
}

void animation_next_slot(animation_t *ctx) {
  struct animation_slot *slot;

  ctx->add_slot++;
  if (ctx->add_slot >= ctx->num_slots) {
    if (ctx->num_slots >= ctx->cap_slots) {
      ctx->cap_slots = ctx->cap_slots * 2;
      ctx->slots = realloc(ctx->slots, ctx->cap_slots * sizeof(*ctx->slots));
    }

    slot = &ctx->slots[ctx->num_slots];
    ctx->num_slots++;
    slot->cap_animations = 10;
    slot->num_animations = 0;
    slot->animations = calloc(slot->cap_animations, sizeof(*slot->animations));
    slot->done = false;
  }
}
void animation_prev_slot(animation_t *ctx) {
  if (ctx->add_slot > 0) {
    ctx->add_slot--;
  }
}

void animation_queue(animation_t *ctx, enum animation_type type, Vector2 start,
                     float rotation, Color tint) {
  Vector2 stop = {-1, -1};

  return animation_queue_moving(ctx, type, start, stop, rotation, tint);
}

void animation_queue_moving(animation_t *ctx, enum animation_type type,
                            Vector2 start, Vector2 stop, float rotation,
                            Color tint) {

  struct animation *ani;

  ani = get_animation(ctx);

  ani->src = &ctx->src[type];
  ani->start = start;
  ani->rotation = rotation;
  ani->tint = tint;
  ani->step = 0;
  ani->player = NULL;

  if (stop.x >= 0 && stop.y >= 0) {
    float diff_x = stop.x - start.x;
    float diff_y = stop.y - start.y;

    ani->diff.x = diff_x / ani->src->frames;
    ani->diff.y = diff_y / ani->src->frames;
  } else {
    ani->diff.x = 0;
    ani->diff.y = 0;
  }

  if (rotation < 0) {
    ani->rotation = atan2f(ani->diff.y, ani->diff.x);
    printf("ROTATION CALC: %f (%f.%f)\n", ani->rotation, ani->diff.x,
           ani->diff.y);
  }
}

void animation_queue_player(animation_t *ctx, enum animation_type type,
                            uint8_t id, Vector2 start, Vector2 stop,
                            float target_angle) {

  struct animation *ani;

  if (id == 0) {
    printf("### Adding player animation!\n");
  }

  ani = get_animation(ctx);

  ani->src = &ctx->src[type];
  ani->start = start;
  ani->rotation = ctx->players[id].angle;
  ani->step = 0;
  ani->player = &ctx->players[id];
  ani->tint = ctx->players[id].color;

  if (stop.x >= 0 && stop.y >= 0) {
    float diff_x = stop.x - start.x;
    float diff_y = stop.y - start.y;

    ani->diff.x = diff_x / ani->src->frames;
    ani->diff.y = diff_y / ani->src->frames;
  } else {
    ani->diff.x = 0;
    ani->diff.y = 0;
  }

  if (target_angle >= 0) {
    float diff_rot = target_angle - ctx->players[id].angle;
    ani->rotation_diff = diff_rot / ani->src->frames;
  }
}

void animation_queue_damage(animation_t *ctx, uint8_t dmg, Vector2 start) {
  struct animation *ani;

  ani = get_animation(ctx);

  ani->start = start;
  ani->rotation = 0;
  ani->tint = RED;
  ani->step = 0;
  ani->src = &ctx->src[ANIMATION_DAMAGE];
  ani->diff.x = 0;
  ani->diff.y = -1;
  ani->player = NULL;

  snprintf(ani->text, 8, "- %u", dmg);
}

bool animation_active(animation_t *ctx) {
  for (int32_t i = 0; i < ctx->num_slots; i++) {
    if (!ctx->slots[i].done) {
      return true;
    }
  }

  return false;
}

static void draw_player(animation_t *ctx, uint8_t id, Rectangle *dest,
                        float angle, bool center) {

  Vector2 origin = {0, 0};
  if (center) {
    origin.x = dest->width / 2;
    origin.y = dest->height / 2;
  }
  // printf("Drawing player at %fx%f (%fx%f)\n", dest->x, dest->y, origin.x,
  // origin.y);
  DrawTexturePro(ctx->player_image, ctx->players[id].source, *dest, origin,
                 angle, ctx->players[id].color);
}

void animation_draw_players(animation_t *ctx) {
  for (int32_t i = 0; i < ctx->player_count; i++) {

    if (ctx->players[i].dest.x < 0 || ctx->players[i].dest.y < 0) {
      continue;
    }

    draw_player(ctx, i, &ctx->players[i].dest, ctx->players[i].angle, true);
  }
}

void animation_draw_player(animation_t *ctx, uint8_t id, float x, float y) {
  Rectangle dest = {.x = x, .y = y};

  dest.width = ctx->players[id].dest.width;
  dest.height = ctx->players[id].dest.height;

  draw_player(ctx, id, &dest, 0, false);
}

void animation_tick(animation_t *ctx) {
  struct animation_slot *slot = NULL;

  for (int32_t i = 0; i < ctx->num_slots; i++) {
    if (!ctx->slots[i].done) {
      slot = &ctx->slots[i];
      printf("Animating slot %d\n", i);
      break;
    }
  }

  if (slot == NULL) {
    printf("Nothing to animate\n");
    animation_draw_players(ctx);
    return;
  }

  for (uint32_t i = 0; i < ctx->player_count; i++) {
    ctx->players[i].animated = false;
  }

  for (int32_t i = 0; i < slot->num_animations; i++) {
    struct animation *ani = &slot->animations[i];
    if (ani->player != NULL && ani->step < ani->src->frames) {
      ani->player->animated = true;
    }
  }

  for (uint8_t i = 0; i < ctx->player_count; i++) {
    if (ctx->players[i].dest.x < 0 || ctx->players[i].dest.y < 0 ||
        ctx->players[i].animated) {
      if (i == 0) {
        printf("  skipping draw player: (%f | %f) %s\n", ctx->players[i].dest.x,
               ctx->players[i].dest.y,
               ctx->players[i].animated ? "ANIMATED" : "NOT ANIMATED");
      }
      continue;
    }
    draw_player(ctx, i, &ctx->players[i].dest, ctx->players[i].angle, true);
  }

  slot->done = true;
  for (int32_t i = 0; i < slot->num_animations; i++) {
    struct animation *ani = &slot->animations[i];
    Rectangle source;
    Rectangle target;
    Vector2 origin;

    if (ani->step >= ani->src->frames) {
      continue;
    }
    slot->done = false;

    if (ani->src->id == ANIMATION_DAMAGE) {
      DrawText(ani->text, ani->start.x, ani->start.y, 20, ani->tint);
    } else {

      source.width = ani->src->tile_width;
      source.height = ani->src->tile_height;
      source.x = ani->step * ani->src->tile_width;
      source.y = 0;

      target.width = ani->src->tile_width;
      target.height = ani->src->tile_height;
      target.x = ani->start.x;
      target.y = ani->start.y;
      origin.x = target.width / 2;
      origin.y = target.height / 2;
      switch (ani->src->id) {
      case ANIMATION_PLAYER_DEATH:
      case ANIMATION_PLAYER_MOVE_ONE:
        ani->player->dest.x = -1;
        ani->player->dest.y = -1;
        break;
      case ANIMATION_PLAYER_MOVED:
      case ANIMATION_PLAYER_MOVE_TWO:
        ani->player->dest.x = ani->start.x;
        ani->player->dest.y = ani->start.y;
        break;
      default:
        break;
      }

      DrawTexturePro(ani->src->texture, source, target, origin, ani->rotation,
                     ani->tint);
    }

    if (ctx->fps == 60) {
      ctx->draw = !ctx->draw;
      if (ctx->draw) {
        ani->start.x += ani->diff.x;
        ani->start.y += ani->diff.y;
        ani->step++;
      }
    }
  }
}

void animation_clean(animation_t *ctx) {

  for (int32_t i = 0; i < ctx->num_slots; i++) {
    free(ctx->slots[i].animations);
    ctx->slots[i].num_animations = 0;
    ctx->slots[i].cap_animations = 0;
  }

  ctx->num_slots = 0;
  ctx->add_slot = -1;

  animation_next_slot(ctx);
}
