
#include <raylib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "animation.h"
#include "asset.h"
#include "common.h"
#include "engine.h"
#include "incident.h"
#include "map.h"
#include "map_opts.h"
#include "menu.h"
#include "message.h"
#include "player.h"
#include "player_local.h"
#include "player_npc.h"
#include "portals.h"
#include "spell.h"

#define SCREEN_WIDTH 1080
#define SCREEN_HEIGHT 768

#define BUTTON_ID_OK 20
#define BUTTON_ID_NOPE 21

#define SELECT_FACING_RADIUS 200

enum state {
  STATE_SPlASH = 0,
  STATE_MENU_MAIN,
  STATE_MENU_LOCAL,
  STATE_START_LOCAL,
  STATE_MAP_READY,
  STATE_SELECT_SPAWN,
  STATE_SELECT_SPAWN_FACE,
  STATE_SELECT_MOVE,
  STATE_SELECT_MOVE_FACE,
  STATE_SELECT_SPELL,
  STATE_ANIMATING,
  STATE_IN_GAME_WAITING
};

typedef struct {

  uint8_t id;
  int32_t padding;
  int32_t text_width;

  const char *text;
  char bg_text[5];
  int32_t font_size;

  Color color;
  bool visible;

  /* Cached for click detection */
  int32_t x;
  int32_t y;
  int32_t width;
  int32_t height;
} button_t;

typedef struct {
  map_t *map;
  engine_t *engine;
  portals_ctx_t *portals;
  animation_t *animation;

  struct menu_main main_menu;
  struct menu_local local_menu;

  enum state state;
  uint32_t reply_tick;

  int32_t w_width;
  int32_t w_height;
  int32_t margin_left;
  int32_t margin_top;

  int32_t fps;
  int32_t tick;
  uint8_t player_turn;

  player_send_msg_func_t send_msg;
  player_get_msg_func_t get_msg;
  void *msg_ctx;
  message_t *waiting;

  map_opts_t *los_opts;
  map_opts_t *spawn_opts;
  map_opts_t *move_opts;
  map_opts_t *target_opts[PORTAL_NONE];

  bool skip_fight;
  pos_t *selected_square;
  enum portal_type selected_spell;
  enum direction facing;

  player_t *me;
  player_t *players;
  uint8_t player_count;

  bool draw_player_stats;
  button_t player_stats;

  button_t spell_buttons[PORTAL_NONE + 1];
  button_t confirm_buttons[2];

  Texture2D walls;
  Texture2D floor;
  Texture2D portal_base;
  Texture2D portal_top;
} ctx_t;

static int32_t map_x(ctx_t *ctx, uint32_t x) {
  return ctx->margin_left + x * ctx->w_width;
}
static int32_t map_y(ctx_t *ctx, uint32_t y) {
  return ctx->margin_top + y * ctx->w_height;
}

static Color portal_color(enum portal_type kind) {
  switch (kind) {
  case PORTAL_NONE:
    return GRAY;
  case PORTAL_AIR:
    return BLUE;
  case PORTAL_WATER:
    return GREEN;
  case PORTAL_EARTH:
    return YELLOW;
  case PORTAL_FIRE:
    return RED;
  }

  return GRAY;
}

static void setup_draw_portal(ctx_t *ctx) {
  Image img;

  img = asset_get(ASSET_PORTAL1);
  ctx->portal_base = LoadTextureFromImage(img);
  img = asset_get(ASSET_PORTAL2);
  ctx->portal_top = LoadTextureFromImage(img);
}

static void draw_portals(ctx_t *ctx) {
  map_opts_t *list;
  portal_t *p;

  if (ctx->map == NULL || ctx->portals == NULL) {
    return;
  }

  list = map_portals(ctx->map, NULL);

  for (uint32_t i = 0; i < list->size; i++) {
    pos_t pos = list->data[i];
    p = portals_get_at(ctx->portals, pos);
    Color col;
    Rectangle dst;

    if (p->spell == NULL) {
      col = Fade(portal_color(p->kind), .1);
    } else {
      col = Fade(portal_color(p->kind), .7);
    }
    dst.x = map_x(ctx, pos.x) + ctx->w_width / 2;
    dst.y = map_y(ctx, pos.y) + ctx->w_height / 2;
    dst.width = 20;
    dst.height = 20;
    DrawTexturePro(ctx->portal_base, (Rectangle){0, 0, 20, 20}, dst,
                   (Vector2){10, 10}, 0, WHITE);
    DrawEllipse(map_x(ctx, pos.x) + ctx->w_width / 2,
                map_y(ctx, pos.y) + ctx->w_height / 2, 7,12,  col);
    DrawTexturePro(ctx->portal_top, (Rectangle){0, 0, 20, 20}, dst,
                   (Vector2){10, 10}, 0, WHITE);
  }
}

void message_print(message_t *msg) {

  switch (msg->type) {
  case MESSAGE_MAP:

    break;
  case MESSAGE_ASK_SPAWN:
    printf("Ask spawn message:\n");
    break;

  case MESSAGE_ASK_MOVE:
    break;

  case MESSAGE_ASK_FIGHT:
    break;

  case MESSAGE_PLAYER_UPDATE:
    printf("====== Player update ============\n");
    printf("Player\thealth\tkills\tdeaths\tpos\n");
    for (uint8_t i = 0; i < msg->body.player_update.num_others; i++) {
      printf("%u\t%d\t%d\t%d\t(%d,%d)\n",
             msg->body.player_update.others[i].player_id,
             msg->body.player_update.others[i].health,
             msg->body.player_update.others[i].kills,
             msg->body.player_update.others[i].deaths,
             msg->body.player_update.others[i].pos.x,
             msg->body.player_update.others[i].pos.y);
    }
    printf("\n\nPosition\tkind\tspell\n");
    for (uint8_t i = 0; i < msg->body.player_update.num_portals; i++) {
      printf("(%d,%d)\t%u\t%u\n", msg->body.player_update.portals[i].pos.x,
             msg->body.player_update.portals[i].pos.y,
             msg->body.player_update.portals[i].kind,
             msg->body.player_update.portals[i].spell);
    }
    printf("\n\nPosition\tkind\tspell\n");

    for (uint8_t i = 0; i < msg->body.player_update.num_events; i++) {
      printf(
          "%s from (%d,%d), type %s (%s), cast by %u\n",
          incident_type_string(msg->body.player_update.events[i].incident_type),
          msg->body.player_update.events[i].from.x,
          msg->body.player_update.events[i].from.y,
          kind_string(msg->body.player_update.events[i].spell_kind),
          spell_id_to_name(msg->body.player_update.events[i].spell_id),
          msg->body.player_update.events[i].player_origin);

      for (uint8_t j = 0; j < msg->body.player_update.events[i].num_targets;
           j++) {
        printf("\tTarget (%d,%d):\n",
               msg->body.player_update.events[i].targets[j].target.x,
               msg->body.player_update.events[i].targets[j].target.y);
        for (uint8_t k = 0;
             k < msg->body.player_update.events[i].targets[j].num_effects;
             k++) {
          struct applied_effect *eff =
              &msg->body.player_update.events[i].targets[j].effects[k];

          switch (eff->type) {
          case SPELL_EFFECT_DAMAGE:
            printf("\t\t%d Damage at (%d,%d) on player %u\n", eff->data.dmg,
                   eff->at.x, eff->at.y, eff->victim);
            break;
          case SPELL_EFFECT_PUSH:
          case SPELL_EFFECT_PUSH_RANDOM:
            printf("\t\tPushing player %u from (%d,%d) to (%d,%d)\n",
                   eff->victim, eff->at.x, eff->at.y, eff->data.new_pos.x,
                   eff->data.new_pos.y);
            break;
          case SPELL_EFFECT_PULL:
            printf("\t\tPulling player %u from (%d,%d) to (%d,%d)\n",
                   eff->victim, eff->at.x, eff->at.y, eff->data.new_pos.x,
                   eff->data.new_pos.y);
            break;

          default:
            printf("\t\tNon-damage effect\n");
          }
        }
      }
    }
    printf(" ================================================\n");
    break;
  default:
    break;
  }
}

static void setup_spell_buttons(ctx_t *ctx) {

  if (ctx->me == NULL) {
    return;
  }
  for (uint8_t i = 0; i < PORTAL_NONE; i++) {
    ctx->spell_buttons[i].id = i;
    ctx->spell_buttons[i].font_size = 24;
    ctx->spell_buttons[i].padding = 6;
    ctx->spell_buttons[i].visible = true;

    if (ctx->me->spells[i] != NULL) {
      ctx->spell_buttons[i].text = ctx->me->spells[i]->name;
      snprintf(ctx->spell_buttons[i].bg_text, 5, "%d", ctx->me->charges[i]);
      printf("WROTE CHARGETS %d\n", ctx->me->charges[i]);
    } else {
      ctx->spell_buttons[i].text = "Empty";
    }
    if (ctx->me->charges[i] > 0) {
      ctx->spell_buttons[i].color = portal_color(i);
    } else {
      ctx->spell_buttons[i].color = GRAY;
    }
    ctx->spell_buttons[i].text_width = MeasureText(
        ctx->spell_buttons[i].text, ctx->spell_buttons[i].font_size);
  }
  ctx->spell_buttons[PORTAL_NONE].id = PORTAL_NONE;
  ctx->spell_buttons[PORTAL_NONE].font_size = 24;
  ctx->spell_buttons[PORTAL_NONE].padding = 6;
  ctx->spell_buttons[PORTAL_NONE].visible = true;

  ctx->spell_buttons[PORTAL_NONE].text = "Skip attack";
  ctx->spell_buttons[PORTAL_NONE].color = Fade(PINK, 0.7f);
  ctx->spell_buttons[PORTAL_NONE].text_width =
      MeasureText(ctx->spell_buttons[PORTAL_NONE].text,
                  ctx->spell_buttons[PORTAL_NONE].font_size);
}

static void setup_confirm(ctx_t *ctx) {

  if (ctx->confirm_buttons[0].text != NULL) {
    return;
  }

  ctx->confirm_buttons[0].id = BUTTON_ID_OK;
  ctx->confirm_buttons[1].id = BUTTON_ID_NOPE;
  ctx->confirm_buttons[0].text = "confirm";
  ctx->confirm_buttons[1].text = "back";
  ctx->confirm_buttons[0].color = Fade(GREEN, 0.7f);
  ctx->confirm_buttons[1].color = Fade(PINK, 0.7f);

  for (uint8_t i = 0; i < 2; i++) {
    ctx->confirm_buttons[i].padding = 4;
    ctx->confirm_buttons[i].font_size = 14;
    ctx->confirm_buttons[i].visible = false;

    ctx->confirm_buttons[i].text_width = MeasureText(
        ctx->confirm_buttons[i].text, ctx->confirm_buttons[i].font_size);
  }
}
static void setup_player_stats(ctx_t *ctx) {

  if (ctx->player_stats.text != NULL) {
    return;
  }
  ctx->player_stats.id = 0;
  ctx->player_stats.text = "Player stats";
  ctx->player_stats.color = RAYWHITE;
  ctx->player_stats.padding = 5;
  ctx->player_stats.font_size = 20;
  ctx->player_stats.visible = true;

  ctx->player_stats.text_width =
      MeasureText(ctx->player_stats.text, ctx->player_stats.font_size);
}

static void show_confirm(ctx_t *ctx) {
  ctx->confirm_buttons[0].visible = true;
  ctx->confirm_buttons[1].visible = true;
}
static void hide_confirm(ctx_t *ctx) {
  ctx->confirm_buttons[0].visible = false;
  ctx->confirm_buttons[1].visible = false;
}

static void draw_buttons(ctx_t *ctx, button_t *buttons, uint8_t num_buttons,
                         int32_t x, int32_t y, int32_t spacing) {

  int32_t total_width = 0;

  for (uint8_t i = 0; i < num_buttons; i++) {
    total_width += buttons[i].text_width + buttons[i].padding * 2 + spacing;
  }

  total_width -= spacing;

  if (x == -1) {
    x = GetScreenWidth() / 2 - total_width / 2;
  }

  for (uint8_t i = 0; i < num_buttons; i++) {
    int32_t width = buttons[i].padding * 2 + buttons[i].text_width;
    int32_t height = buttons[i].padding * 2 + buttons[i].font_size;

    buttons[i].width = width;
    buttons[i].height = height;
    buttons[i].x = x;
    buttons[i].y = y;

    if (!buttons[i].visible) {
      continue;
    }

    DrawRectangle(x, y, width, height, buttons[i].color);
    DrawRectangleLines(x, y, width, height, BLACK);

    DrawText(buttons[i].bg_text, x + buttons[i].width - 8, y,
             buttons[i].font_size / 4, BLACK);

    DrawText(buttons[i].text, x + buttons[i].padding, y + buttons[i].padding,
             buttons[i].font_size, BLACK);
    x += buttons[i].padding + spacing + buttons[i].text_width;
  }
}

static void init_local(ctx_t *ctx) {
  uint32_t width = 80;
  uint32_t height = 40;

  map_t *map =
      map_new(width, height, (int)ctx->local_menu.walls, ctx->local_menu.seed);

  ctx->msg_ctx = player_local_new();
  ctx->send_msg = player_local_send;
  ctx->get_msg = player_local_get;

  ctx->player_count = (int)ctx->local_menu.players;

  ctx->engine = engine_new(ctx->player_count, map, NULL, false);

  engine_add_player(ctx->engine, player_local_server_send, ctx->msg_ctx,
                    player_local_server_get, ctx->msg_ctx);

  for (uint8_t i = 1; i < ctx->player_count; i++) {
    void *npc = player_npc_new();

    engine_add_player(ctx->engine, player_npc_server_send, npc,
                      player_npc_server_get, npc);
  }
}

static void setup_floor(ctx_t *ctx) {
  Image img;
  coord_t height = map_height(ctx->map);
  coord_t width = map_width(ctx->map);

  img = GenImageColor(width * ctx->w_width, height * ctx->w_height, RAYWHITE);

  ctx->floor = LoadTextureFromImage(img);
  UnloadImage(img);
}

static bool wall_or_out(ctx_t *ctx, coord_t x, coord_t y) {
  pos_t pos = {.x = x, .y = y};

  return map_is_wall(ctx->map, pos) || pos.x < 0 || pos.y < 0 ||
         pos.x >= map_width(ctx->map) || pos.y >= map_height(ctx->map);
}

static void fill_wall(ctx_t *ctx, Image *img, pos_t pos, enum direction dir,
                      Image *tile) {
  Rectangle dst_rec = {.width = 20,
                       .height = 20,
                       .x = pos.x * ctx->w_width,
                       .y = pos.y * ctx->w_height};
  Rectangle src_rec = {.x = (dir + 1) * 20, .y = 0, .width = 20, .height = 20};

  switch (dir) {
  case DIRECTION_NORTH:
    pos.y--;
    break;
  case DIRECTION_SOUTH:
    pos.y++;
    break;
  case DIRECTION_WEST:
    pos.x--;
    break;
  case DIRECTION_EAST:
    pos.x++;
    break;

  case DIRECTION_NORTH_EAST:
    if (wall_or_out(ctx, pos.x, pos.y - 1) &&
        wall_or_out(ctx, pos.x + 1, pos.y) &&
        wall_or_out(ctx, pos.x + 1, pos.y - 1)) {

      ImageDraw(img, *tile, src_rec, dst_rec, GREEN);
    }
    return;
  case DIRECTION_NORTH_WEST:
    if (wall_or_out(ctx, pos.x, pos.y - 1) &&
        wall_or_out(ctx, pos.x - 1, pos.y) &&
        wall_or_out(ctx, pos.x - 1, pos.y - 1)) {

      ImageDraw(img, *tile, src_rec, dst_rec, GREEN);
    }
    return;
  case DIRECTION_SOUTH_WEST:
    if (wall_or_out(ctx, pos.x, pos.y + 1) &&
        wall_or_out(ctx, pos.x - 1, pos.y) &&
        wall_or_out(ctx, pos.x - 1, pos.y + 1)) {

      ImageDraw(img, *tile, src_rec, dst_rec, GREEN);
    }
    return;
  case DIRECTION_SOUTH_EAST:
    if (wall_or_out(ctx, pos.x, pos.y + 1) &&
        wall_or_out(ctx, pos.x + 1, pos.y) &&
        wall_or_out(ctx, pos.x + 1, pos.y + 1)) {

      ImageDraw(img, *tile, src_rec, dst_rec, GREEN);
    }
    return;

  default:
    return;
  }

  if (wall_or_out(ctx, pos.x, pos.y)) {
    ImageDraw(img, *tile, src_rec, dst_rec, GREEN);
  }
}

static void setup_walls(ctx_t *ctx) {
  Image img;
  Image tile;
  coord_t height = map_height(ctx->map);
  coord_t width = map_width(ctx->map);
  Rectangle src_rec = {.x = 0, .y = 0, .width = 20, .height = 20};

  img = GenImageColor(width * ctx->w_width, height * ctx->w_height, BLANK);

  tile = asset_get(ASSET_WALL);

  for (coord_t y = 0; y < height; y++) {
    for (coord_t x = 0; x < width; x++) {
      pos_t p = {x, y};
      if (map_is_wall(ctx->map, p)) {
        Rectangle dst_rec = {.x = x * ctx->w_width,
                             .y = y * ctx->w_height,
                             .width = 20,
                             .height = 20};

        // ImageDrawRectangle(&img, x * ctx->w_width, y * ctx->w_height,
        //                  ctx->w_width, ctx->w_height, BLACK);

        ImageDraw(&img, tile, src_rec, dst_rec, WHITE);
        for (uint8_t i = 0; i < DIRECTION_ANY; i++) {
          fill_wall(ctx, &img, p, i, &tile);
        }
      }
    }
  }

  ctx->walls = LoadTextureFromImage(img);
  UnloadImage(img);
}

static void draw_floor(ctx_t *ctx) {
  DrawTexture(ctx->floor, ctx->margin_left, ctx->margin_top, WHITE);
}
static void draw_walls(ctx_t *ctx) {
  DrawTexture(ctx->walls, ctx->margin_left, ctx->margin_top, WHITE);
}

static void draw_fog_of_war(ctx_t *ctx) {
  coord_t w;
  coord_t h;
  if (ctx->los_opts == NULL) {
    return;
  }

  w = map_width(ctx->map);
  h = map_height(ctx->map);

  for (coord_t x = 0; x < w; x++) {
    for (coord_t y = 0; y < h; y++) {
      pos_t p = {x, y};
      if (!map_opts_contains(ctx->los_opts, p)) {
        DrawRectangle(map_x(ctx, p.x), map_y(ctx, p.y), ctx->w_width,
                      ctx->w_height, Fade(BLACK, .5f));
      }
    }
  }
}

static void draw_select_square(ctx_t *ctx, map_opts_t *opts) {

  if (opts == NULL) {
    return;
  }
  for (uint32_t i = 0; i < opts->size; i++) {
    pos_t p = opts->data[i];
    DrawRectangle(map_x(ctx, p.x), map_y(ctx, p.y), ctx->w_width, ctx->w_height,
                  Fade(SKYBLUE, .5f));
  }
}

static void draw_selected_square(ctx_t *ctx) {

  if (ctx->selected_square == NULL) {
    return;
  }
  DrawRectangle(map_x(ctx, ctx->selected_square->x),
                map_y(ctx, ctx->selected_square->y), ctx->w_width,
                ctx->w_height, Fade(BLUE, .5f));
}

static void draw_attack_info(ctx_t *ctx) {
  int32_t box_x = 200;
  int32_t box_y = 50;
  int32_t box_width = 100;
  int32_t box_height = 200;
  int32_t margin = 10;
  int32_t text_height = 16;
  int32_t lines = 0;
  char text[15][200];

  const spell_t *spell;
  coord_t dist_square;
  int8_t hit;
  int8_t dmg_min;
  int8_t dmg_max;

  if (ctx->selected_spell == PORTAL_NONE || ctx->selected_square == NULL ||
      ctx->me->spells[ctx->selected_spell] == NULL) {
    return;
  }
  spell = ctx->me->spells[ctx->selected_spell];
  dist_square =
      map_distance_squared(ctx->map, ctx->me->position, *ctx->selected_square);
  spell_get_stats(spell, dist_square, &hit, &dmg_min, &dmg_max);

  snprintf(text[lines], 200, "Spell: %s (%s)", spell->name,
           kind_string(spell->kind));
  lines++;
  snprintf(text[lines], 200, "Charges: %d", ctx->me->charges[spell->kind]);
  lines++;
  snprintf(text[lines], 200, "Speed: %d", spell->speed);
  lines++;
  snprintf(text[lines], 200, "Bursts: %d", spell->burst);
  lines++;
  snprintf(text[lines], 200, "Hit: %d %%", hit);
  lines++;
  snprintf(text[lines], 200, "Damage: %d - %d", dmg_min, dmg_max);
  lines++;

  for (uint8_t i = 0; i < ctx->player_count; i++) {
    player_t *p = &ctx->players[i];
    if (POS_EQ(p->position, (*ctx->selected_square))) {
      snprintf(text[lines], 200, "Target health: %d", p->health);
      lines++;
    }
  }
  for (uint8_t i = 0; i < spell->num_effects; i++) {
    const struct spell_effect *eff = &spell->effect[i];
    switch (eff->type) {
    case SPELL_EFFECT_NONE:
    case SPELL_EFFECT_DAMAGE:
      break;
    case SPELL_EFFECT_BE_HIT_MOD:
      snprintf(text[lines], 200, "Modifier to be hit: %d (lasts %d turns)",
               eff->params.mod.value, eff->params.mod.duration);
      lines++;
      break;
    case SPELL_EFFECT_HIT_MOD:
      snprintf(text[lines], 200, "Modifier to hit targets: %d (lasts %d turns)",
               eff->params.mod.value, eff->params.mod.duration);
      lines++;
      break;
    case SPELL_EFFECT_DAMAGE_MOD:
      snprintf(text[lines], 200,
               "Modifier incoming damage: %d (lasts %d turns)",
               eff->params.mod.value, eff->params.mod.duration);
      lines++;
      break;
    case SPELL_EFFECT_HEAL:
      snprintf(text[lines], 200, "Heals %d-%d percentage points",
               eff->params.heal.min, eff->params.heal.max);
      lines++;
      break;
    case SPELL_EFFECT_PUSH:
      snprintf(text[lines], 200, "Pushes target %d-%d squares away",
               eff->params.move.min, eff->params.move.max);
      lines++;
      break;
    case SPELL_EFFECT_PULL:
      snprintf(text[lines], 200, "Pulls target %d-%d squares closer",
               eff->params.move.min, eff->params.move.max);
      lines++;
      break;
    case SPELL_EFFECT_PUSH_RANDOM:
      snprintf(text[lines], 200,
               "Moves target %d-%d squares in random direction",
               eff->params.move.min, eff->params.move.max);
      lines++;
      break;
    case SPELL_EFFECT_POISON:
      snprintf(text[lines], 200, "Poisons target for %d-%d damage for %d turns",
               eff->params.poison.min, eff->params.poison.max,
               eff->params.poison.duration);
      lines++;
      break;
    case SPELL_EFFECT_SPLASH:
      snprintf(text[lines], 200,
               "Splashes for %d-%d damage, loses %d per %d distance",
               eff->params.splash.dmg.min, eff->params.splash.dmg.max,
               eff->params.splash.drop_of, eff->params.splash.radius_step);
      lines++;
      break;
    }
  }

  for (uint8_t i = 0; i < lines; i++) {
    int32_t w;

    w = MeasureText(text[i], text_height);
    if (w > box_width) {
      box_width = w;
    }
  }

  box_width += margin * 2;
  box_height = lines * (text_height + 2) + margin * 2;

  DrawRectangle(box_x, box_y, box_width, box_height, Fade(RAYWHITE, .7f));
  DrawRectangleLines(box_x, box_y, box_width, box_height, BLACK);

  for (uint8_t i = 0; i < lines; i++) {
    DrawText(text[i], box_x + margin, box_y + margin + (i * text_height + 2),
             text_height, BLACK);
  }
}

static void draw_player_stats(ctx_t *ctx) {
  int32_t width = 0;
  int32_t height;
  draw_buttons(ctx, &ctx->player_stats, 1, 40, 5, 0);

  if (!ctx->draw_player_stats) {
    return;
  }

  height = 20 * ctx->player_count + 2 * 5;
  for (uint8_t i = 0; i < ctx->player_count; i++) {
    char text[50];
    int32_t w;
    player_t *p = &ctx->players[i];
    snprintf(
        text, 50, " %d / %d   %d%% %s | %s | %s | %s", p->kills, p->deaths,
        p->health,
        p->spells[PORTAL_AIR] != NULL ? p->spells[PORTAL_AIR]->name : "None",
        p->spells[PORTAL_FIRE] != NULL ? p->spells[PORTAL_FIRE]->name : "None",
        p->spells[PORTAL_WATER] != NULL ? p->spells[PORTAL_WATER]->name
                                        : "None",
        p->spells[PORTAL_EARTH] != NULL ? p->spells[PORTAL_EARTH]->name
                                        : "None");

    w = MeasureText(text, 18);
    if (w > width) {
      width = w;
    }
  }

  width = width + 2 * 5 + 20 + 5;

  DrawRectangle(40, 40, width, height, RAYWHITE);
  DrawRectangleLines(40, 40, width, height, BLACK);
  for (uint8_t i = 0; i < ctx->player_count; i++) {
    char text[50];
    player_t *p = &ctx->players[i];
    float x;
    float y;
    snprintf(
        text, 50, " %d / %d   %d%% %s | %s | %s | %s", p->kills, p->deaths,
        p->health,
        p->spells[PORTAL_AIR] != NULL ? p->spells[PORTAL_AIR]->name : "None",
        p->spells[PORTAL_FIRE] != NULL ? p->spells[PORTAL_FIRE]->name : "None",
        p->spells[PORTAL_WATER] != NULL ? p->spells[PORTAL_WATER]->name
                                        : "None",
        p->spells[PORTAL_EARTH] != NULL ? p->spells[PORTAL_EARTH]->name
                                        : "None");

    x = 40 + 5;
    y = 40 + 5 + i * 20;

    animation_draw_player(ctx->animation, i, x, y);
    DrawText(text, x + 20 + 5, y, 18, BLACK);
  }
}

static void player_stats_visible(ctx_t *ctx) {
  int32_t x = GetMouseX();
  int32_t y = GetMouseY();

  if ((x > ctx->player_stats.x &&
       x <= ctx->player_stats.x + ctx->player_stats.width) &&
      (y > ctx->player_stats.y &&
       y <= ctx->player_stats.y + ctx->player_stats.height)) {
    ctx->draw_player_stats = !ctx->draw_player_stats;
    printf("Toggled player stats");
  }
}

static void handle_waiting_player_update(ctx_t *ctx) {
  message_t *reply;

  if (ctx->waiting == NULL) {
    return;
  }

  player_batch_update(ctx->players, ctx->player_count, ctx->waiting);
  for (uint8_t i = 0; i < ctx->player_count; i++) {
    float x;
    float y;
    float angle;

    if (POS_IS_UNKNOWN(ctx->players[i].position)) {
      x = -1;
      y = -1;
      angle = 0;
    } else {
      x = map_x(ctx, ctx->players[i].position.x) + 10;
      y = map_y(ctx, ctx->players[i].position.y) + 10;
      angle = ctx->players[i].facing * 45;
    }

    animation_set_player(ctx->animation, i, x, y, angle);
  }
  portals_update(ctx->portals, ctx->waiting);

  setup_spell_buttons(ctx);
  setup_player_stats(ctx);

  map_opts_free(ctx->los_opts);
  ctx->los_opts = map_opts_import(ctx->waiting->body.player_update.los.opts,
                                  ctx->waiting->body.player_update.los.size);

  ctx->skip_fight = ctx->waiting->body.player_update.num_others == 0;
  if (ctx->skip_fight) {
    show_confirm(ctx);
  }

  reply = message_reply_player_update(ctx->waiting->tick);
  ctx->send_msg(ctx->msg_ctx, reply);

  message_unref(ctx->waiting);
  message_unref(reply);

  ctx->waiting = NULL;
}

static void handle_player_update(ctx_t *ctx, message_t *msg) {

  message_print(msg);

  ctx->state = STATE_ANIMATING;
  ctx->waiting = message_ref(msg);

  for (uint8_t i = 0; i < msg->body.player_update.num_events; i++) {

    printf("Got event: %u %s (%d,%d)\n", i,
           msg->body.player_update.events[i].incident_type == INCIDENT_PORTAL
               ? "Portal"
               : "Spell",
           msg->body.player_update.events[i].from.x,
           msg->body.player_update.events[i].from.y);

    switch (msg->body.player_update.events[i].incident_type) {
    case INCIDENT_PORTAL: {
      Vector2 start;
      portal_t *portal;
      pos_t pos;

      pos.x = msg->body.player_update.events[i].from.x;
      pos.y = msg->body.player_update.events[i].from.y;
      start.x = map_x(ctx, pos.x) + 10;
      start.y = map_y(ctx, pos.y) + 10;

      portal = portals_get_at(ctx->portals, pos);

      printf("ANIMATING PORTAL\n");
      animation_queue(ctx->animation, ANIMATION_PORTAL_USE, start, 0,
                      portal_color(portal->kind));
      break;
    }

    case INCIDENT_PLAYER_MOVE: {
      Vector2 stop = {-1, -1};
      Vector2 start;
      pos_t pos;
      uint8_t player;

      pos.x = msg->body.player_update.events[i].from.x;
      pos.y = msg->body.player_update.events[i].from.y;
      start.x = map_x(ctx, pos.x) + 10;
      start.y = map_y(ctx, pos.y) + 10;
      player = msg->body.player_update.events[i].player_origin;
      animation_queue_player(ctx->animation, ANIMATION_PLAYER_MOVE_ONE, player,
                             start, stop, ctx->players[player].facing * 45);
      animation_next_slot(ctx->animation);
      pos.x = msg->body.player_update.events[i].targets[0].target.x;
      pos.y = msg->body.player_update.events[i].targets[0].target.y;
      start.x = map_x(ctx, pos.x) + 10;
      start.y = map_y(ctx, pos.y) + 10;
      animation_queue_player(ctx->animation, ANIMATION_PLAYER_MOVE_TWO, player,
                             start, stop, ctx->players[player].facing * 45);
      animation_prev_slot(ctx->animation);
      break;
    }

    case INCIDENT_PLAYER_KILLED: {
      Vector2 start;
      Vector2 stop = {-1, -1};
      pos_t pos;

      pos.x = msg->body.player_update.events[i].from.x;
      pos.y = msg->body.player_update.events[i].from.y;
      start.x = map_x(ctx, pos.x) + 10;
      start.y = map_y(ctx, pos.y) + 10;

      if (msg->body.player_update.events[i].player_origin != PLAYER_UNKNOWN) {
        animation_next_slot(ctx->animation);
        animation_queue_player(ctx->animation, ANIMATION_PLAYER_DEATH,
                               msg->body.player_update.events[i].player_origin,
                               start, stop, -1);
      }

      break;
    }

    case INCIDENT_SPELL: {

      Vector2 start;
      Vector2 stop;
      const spell_t *spell;
      pos_t pos;

      pos.x = msg->body.player_update.events[i].from.x;
      pos.y = msg->body.player_update.events[i].from.y;
      start.x = map_x(ctx, pos.x) + 10;
      start.y = map_y(ctx, pos.y) + 10;

      animation_next_slot(ctx->animation);
      animation_queue(
          ctx->animation, ANIMATION_SPELL_AURA, start, 0,
          portal_color(msg->body.player_update.events[i].spell_kind));

      printf("NUM TARGETS: %u\n",
             msg->body.player_update.events[i].num_targets);

      for (uint8_t j = 0; j < msg->body.player_update.events[i].num_targets;
           j++) {
        struct target *targ = &msg->body.player_update.events[i].targets[j];
        pos.x = targ->target.x;
        pos.y = targ->target.y;
        stop.x = map_x(ctx, pos.x) + 10;
        stop.y = map_y(ctx, pos.y) + 10;

        animation_queue_moving(
            ctx->animation, ANIMATION_GROWING_BALL, start, stop, -1,
            portal_color(msg->body.player_update.events[i].spell_kind));

        for (uint8_t k = 0; k < targ->num_effects; k++) {
          struct applied_effect *eff = &targ->effects[k];

          switch (eff->type) {
          case SPELL_EFFECT_DAMAGE: {
            Vector2 start;
            printf("ANIMATING DAMAGE: %d\n", eff->data.dmg);

            start.x = map_x(ctx, eff->at.x) + 10;
            start.y = map_y(ctx, eff->at.y) + 10;
            animation_next_slot(ctx->animation);
            animation_queue_damage(ctx->animation, eff->data.dmg, start);

            break;
          }
          case SPELL_EFFECT_PULL:
          case SPELL_EFFECT_PUSH:
          case SPELL_EFFECT_PUSH_RANDOM: {
            Vector2 start;
            Vector2 stop;

            start.x = map_x(ctx, eff->at.x) + 10;
            start.y = map_y(ctx, eff->at.y) + 10;
            stop.x = map_x(ctx, eff->data.new_pos.x) + 10;
            stop.y = map_y(ctx, eff->data.new_pos.y) + 10;
            animation_next_slot(ctx->animation);
            animation_queue_player(ctx->animation, ANIMATION_PLAYER_MOVED,
                                   eff->victim, start, stop, -1);
            break;
          }
          }
        }
      }

      spell = spell_get_by_id(msg->body.player_update.events[i].spell_id);

      printf("ANIMATING SPELL\n");
      if (spell != NULL) {

      } else {
        printf("SPELL WAS NULL\n");
      }
      break;
    }
    }
  }
}

static void handle_message(ctx_t *ctx) {
  message_t *msg;
  message_t *reply = NULL;

  if (ctx->msg_ctx == NULL) {
    /* Not set up to receive messages yet */
    return;
  }

  msg = ctx->get_msg(ctx->msg_ctx);
  if (msg == NULL) {
    return;
  }

  if (ctx->animation == NULL) {
    ctx->animation = animation_new(ctx->fps, ctx->player_count);
  }

  ctx->tick = msg->tick;

  switch (msg->type) {
  case MESSAGE_ASK_READY:
    reply = message_reply_ready(msg->tick);
    break;

  case MESSAGE_MAP:
    ctx->map = map_new_from_message(msg);
    ctx->portals = portals_new_from_message(msg);
    ctx->player_count = msg->body.map.num_players;
    ctx->players = player_create(ctx->player_count);
    setup_floor(ctx);
    setup_walls(ctx);
    setup_draw_portal(ctx);
    ctx->animation = animation_new(ctx->fps, ctx->player_count);

    reply = message_reply_map(msg->tick);
    ctx->state = STATE_MAP_READY;
    break;

  case MESSAGE_ASK_SPAWN:
    map_opts_free(ctx->spawn_opts);

    ctx->me = &ctx->players[msg->body.ask_spawn.player_id];
    ctx->spawn_opts = map_opts_new(msg->body.ask_spawn.size);

    for (uint8_t i = 0; i < msg->body.ask_spawn.size; i++) {
      map_opts_add(ctx->spawn_opts, msg->body.ask_spawn.opts[i]);
    }

    ctx->selected_square = NULL;
    ctx->facing = DIRECTION_ANY;
    ctx->reply_tick = msg->tick;
    ctx->state = STATE_SELECT_SPAWN;
    break;

  case MESSAGE_ASK_MOVE:
    printf("Got ask move cmd (%u)\n", msg->body.ask_move.size);
    map_opts_free(ctx->move_opts);

    ctx->move_opts =
        map_opts_import(msg->body.ask_move.opts, msg->body.ask_move.size);

    ctx->selected_square = NULL;
    ctx->facing = DIRECTION_ANY;
    ctx->reply_tick = msg->tick;
    ctx->state = STATE_SELECT_MOVE;
    break;

  case MESSAGE_ASK_FIGHT:

    for (uint8_t i = 0; i < PORTAL_NONE; i++) {
      map_opts_free(ctx->target_opts[i]);

      ctx->target_opts[i] =
          map_opts_import(msg->body.ask_fight.spell_opts[i].opts,
                          msg->body.ask_fight.spell_opts[i].size);
    }

    ctx->selected_spell = PORTAL_NONE;
    ctx->selected_square = NULL;
    ctx->facing = DIRECTION_ANY;
    ctx->reply_tick = msg->tick;
    ctx->state = STATE_SELECT_SPELL;
    break;

  case MESSAGE_PLAYER_UPDATE:
    handle_player_update(ctx, msg);
    break;

  default:
    break;
  }

  if (reply != NULL) {
    ctx->send_msg(ctx->msg_ctx, reply);
  }
  message_unref(msg);
  message_unref(reply);
}

static pos_t *get_selected_pos(ctx_t *ctx, map_opts_t *opts) {
  int32_t x = GetMouseX();
  int32_t y = GetMouseY();

  for (uint32_t i = 0; i < opts->size; i++) {
    pos_t *p = &opts->data[i];
    int32_t mx = map_x(ctx, p->x);
    int32_t my = map_y(ctx, p->y);

    if (x >= mx && y >= my && x <= (mx + ctx->w_width) &&
        y <= (my + ctx->w_height)) {
      return p;
    }
  }
  return NULL;
}

static button_t *get_selected_button(ctx_t *ctx) {
  int32_t x = GetMouseX();
  int32_t y = GetMouseY();

  for (uint8_t i = 0; i <= PORTAL_NONE; i++) {
    button_t *b = &ctx->spell_buttons[i];

    if (!b->visible) {
      continue;
    }

    if (x >= b->x && y >= b->y && x <= (b->x + b->width) &&
        y <= (b->y + b->height)) {
      return b;
    }
  }

  for (uint8_t i = 0; i < 2; i++) {
    button_t *b = &ctx->confirm_buttons[i];
    if (!b->visible) {
      continue;
    }

    printf("Click in (%d,%d) vs (%d,%d) -> %dx%d\n", x, y, b->x, b->y, b->width,
           b->height);

    if (x >= b->x && y >= b->y && x <= (b->x + b->width) &&
        y <= (b->y + b->height)) {
      printf("  FOUND\n");
      return b;
    }
  }
  return NULL;
}

static bool within_distance(float dist, Vector2 from, Vector2 to) {

  return (to.x - from.x) * (to.x - from.x) + (to.y - from.y) * (to.y - from.y) <
         (dist * dist);
}
static enum direction get_selected_facing(ctx_t *ctx) {

  Vector2 from = GetMousePosition();

  float s_h = GetScreenHeight();
  float s_w = GetScreenWidth();

  Vector2 nw = {0, 0};
  Vector2 n = {s_w / 2, 0};
  Vector2 ne = {s_w - 1, 0};
  Vector2 w = {0, s_h / 2};
  Vector2 e = {s_w - 1, s_h / 2};
  Vector2 sw = {0, s_h - 1};
  Vector2 s = {s_w / 2, s_h - 1};
  Vector2 se = {s_w - 1, s_h - 1};

  if (within_distance(SELECT_FACING_RADIUS, from, nw)) {
    return DIRECTION_NORTH_WEST;
  }

  if (within_distance(SELECT_FACING_RADIUS, from, n)) {
    return DIRECTION_NORTH;
  }

  if (within_distance(SELECT_FACING_RADIUS, from, ne)) {
    return DIRECTION_NORTH_EAST;
  }

  if (within_distance(SELECT_FACING_RADIUS, from, w)) {
    return DIRECTION_WEST;
  }

  if (within_distance(SELECT_FACING_RADIUS, from, e)) {
    return DIRECTION_EAST;
  }

  if (within_distance(SELECT_FACING_RADIUS, from, sw)) {
    return DIRECTION_SOUTH_WEST;
  }

  if (within_distance(SELECT_FACING_RADIUS, from, s)) {
    return DIRECTION_SOUTH;
  }

  if (within_distance(SELECT_FACING_RADIUS, from, se)) {
    return DIRECTION_SOUTH_EAST;
  }

  return DIRECTION_ANY;
}

static void handle_click(ctx_t *ctx) {
  pos_t *tmp = NULL;
  button_t *button = NULL;
  enum direction dir = DIRECTION_ANY;

  if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
    setup_confirm(ctx);
    /* Find out what's clicked */
    switch (ctx->state) {
    case STATE_SELECT_MOVE:
      tmp = get_selected_pos(ctx, ctx->move_opts);
      button = get_selected_button(ctx);
      if (tmp != NULL) {
        ctx->selected_square = tmp;
        show_confirm(ctx);
      }
      if (button != NULL) {
        if (button->id == BUTTON_ID_OK && ctx->selected_square != NULL) {
          hide_confirm(ctx);
          ctx->state = STATE_SELECT_MOVE_FACE;
        } else if (button->id == BUTTON_ID_NOPE) {
          hide_confirm(ctx);
          ctx->selected_square = NULL;
        }
      }
      player_stats_visible(ctx);
      break;

    case STATE_SELECT_MOVE_FACE:
      dir = get_selected_facing(ctx);
      button = get_selected_button(ctx);

      if (button != NULL) {
        if (button->id == BUTTON_ID_OK && ctx->facing != DIRECTION_ANY &&
            ctx->selected_square != NULL) {
          message_t *reply;

          printf("Sending move reply (face %u)!\n", ctx->facing);
          reply = message_reply_move(ctx->reply_tick, *ctx->selected_square,
                                     ctx->facing);
          ctx->send_msg(ctx->msg_ctx, reply);
          ctx->state = STATE_IN_GAME_WAITING;
          hide_confirm(ctx);
          message_unref(reply);
          break;
        } else if (button->id == BUTTON_ID_NOPE) {
          hide_confirm(ctx);
          ctx->facing = DIRECTION_ANY;
          break;
        }
      }
      if (dir != DIRECTION_ANY) {
        ctx->facing = dir;
        show_confirm(ctx);
      }
      break;

    case STATE_SELECT_SPAWN:

      tmp = get_selected_pos(ctx, ctx->spawn_opts);
      button = get_selected_button(ctx);

      if (tmp != NULL) {
        ctx->selected_square = tmp;
        show_confirm(ctx);
      }

      if (button != NULL) {
        if (button->id == BUTTON_ID_OK && ctx->selected_square != NULL) {
          ctx->state = STATE_SELECT_SPAWN_FACE;
          hide_confirm(ctx);
        } else if (button->id == BUTTON_ID_NOPE) {
          ctx->selected_square = NULL;
          hide_confirm(ctx);
        }
      }
      player_stats_visible(ctx);
      break;

    case STATE_SELECT_SPAWN_FACE:
      dir = get_selected_facing(ctx);
      button = get_selected_button(ctx);

      if (button != NULL) {
        if (button->id == BUTTON_ID_OK && ctx->facing != DIRECTION_ANY &&
            ctx->selected_square != NULL) {
          message_t *reply;
          printf("Sending spawn reply (face %u)!\n", ctx->facing);
          reply = message_reply_spawn(ctx->reply_tick, *ctx->selected_square,
                                      ctx->facing);
          ctx->send_msg(ctx->msg_ctx, reply);

          ctx->state = STATE_IN_GAME_WAITING;
          message_unref(reply);
          hide_confirm(ctx);
          break;
        } else if (button->id == BUTTON_ID_NOPE) {
          hide_confirm(ctx);
          ctx->facing = DIRECTION_ANY;
          break;
        }
      }

      if (dir != DIRECTION_ANY) {
        ctx->facing = dir;
        show_confirm(ctx);
      }
      break;

    case STATE_SELECT_SPELL:
      button = get_selected_button(ctx);

      if (button != NULL) {
        if (button->id < PORTAL_NONE) {
          ctx->selected_spell = button->id;
        } else if (button->id == PORTAL_NONE) {
          printf("Clicked skip fight\n");
          ctx->skip_fight = true;
          show_confirm(ctx);

        } else if (button->id == BUTTON_ID_OK && ctx->skip_fight) {
          message_t *reply;

          reply = message_reply_fight(ctx->reply_tick, 0, POSITION_UNKNOWN);
          ctx->send_msg(ctx->msg_ctx, reply);
          ctx->state = STATE_IN_GAME_WAITING;
          message_unref(reply);
          hide_confirm(ctx);

        } else if (button->id == BUTTON_ID_NOPE && ctx->skip_fight) {
          ctx->skip_fight = false;
          hide_confirm(ctx);
        } else if (button->id == BUTTON_ID_OK && ctx->selected_square != NULL) {
          /* Send the command */
          message_t *reply;
          const spell_t *spell = ctx->me->spells[ctx->selected_spell];

          reply = message_reply_fight(ctx->reply_tick, spell->id,
                                      *ctx->selected_square);
          printf("Sending FIGHT REPLY (%d,%d): %s (%s)\n",
                 (*ctx->selected_square).x, (*ctx->selected_square).y,
                 spell->name, kind_string(spell->kind));
          ctx->send_msg(ctx->msg_ctx, reply);
          ctx->state = STATE_IN_GAME_WAITING;
          message_unref(reply);
          hide_confirm(ctx);
        } else if (button->id == BUTTON_ID_NOPE) {
          ctx->selected_square = NULL;
          hide_confirm(ctx);
        }
      }

      if (ctx->selected_spell != PORTAL_NONE) {
        tmp = get_selected_pos(ctx, ctx->target_opts[ctx->selected_spell]);
        if (tmp != NULL) {
          ctx->selected_square = tmp;
          show_confirm(ctx);
        }
      }

      player_stats_visible(ctx);
      break;

    default:
      break;
    }
  }
}

static void draw_title(const char *txt) {
  int32_t x = GetScreenWidth() / 2 - MeasureText(txt, 18) / 2;
  DrawText(txt, x, 0, 18, BLACK);
}

static void draw_health(ctx_t *ctx) {
  char txt[10];
  Color color;

  if (ctx->me == NULL) {
    return;
  }

  if (ctx->me->health < 20) {
    color = RED;
  } else if (ctx->me->health < 50) {
    color = YELLOW;
  } else {
    color = GREEN;
  }

  snprintf(txt, 9, "%d%%", ctx->me->health);

  DrawText(txt, 10, GetScreenHeight() - 40, 40, color);
}

static void draw_select_orientation(ctx_t *ctx) {
  float radius = SELECT_FACING_RADIUS;
  Color color = Fade(GREEN, .5);

  switch (ctx->facing) {
  case DIRECTION_NORTH_WEST:
    DrawCircle(0, 0, radius, color);
    break;
  case DIRECTION_NORTH:
    DrawCircle(GetScreenWidth() / 2, 0, radius, color);
    break;
  case DIRECTION_NORTH_EAST:
    DrawCircle(GetScreenWidth() - 1, 0, radius, color);
    break;
  case DIRECTION_WEST:
    DrawCircle(0, GetScreenHeight() / 2, radius, color);
    break;
  case DIRECTION_EAST:
    DrawCircle(GetScreenWidth() - 1, GetScreenHeight() / 2, radius, color);
    break;
  case DIRECTION_SOUTH_WEST:
    DrawCircle(0, GetScreenHeight() - 1, radius, color);
    break;
  case DIRECTION_SOUTH:
    DrawCircle(GetScreenWidth() / 2, GetScreenHeight() - 1, radius, color);
    break;
  case DIRECTION_SOUTH_EAST:
    DrawCircle(GetScreenWidth() - 1, GetScreenHeight() - 1, radius, color);
    break;
  case DIRECTION_ANY:
    DrawCircle(0, 0, radius, color);
    DrawCircle(GetScreenWidth() / 2, 0, radius, color);
    DrawCircle(GetScreenWidth() - 1, 0, radius, color);
    DrawCircle(0, GetScreenHeight() / 2, radius, color);
    DrawCircle(GetScreenWidth() - 1, GetScreenHeight() / 2, radius, color);
    DrawCircle(0, GetScreenHeight() - 1, radius, color);
    DrawCircle(GetScreenWidth() / 2, GetScreenHeight() - 1, radius, color);
    DrawCircle(GetScreenWidth() - 1, GetScreenHeight() - 1, radius, color);
  }
}

int main(int argc, char **argv) {
  uint32_t width = 80;
  uint32_t height = 40;
  ctx_t ctx = {0};

  ctx.w_width = 20;
  ctx.w_height = 20;
  ctx.margin_top = 40;
  ctx.margin_left = 10;
  ctx.fps = 60;

  ctx.state = STATE_MENU_MAIN;

  InitWindow(ctx.margin_left * 2 + width * ctx.w_width,
             ctx.margin_top * 2 + height * ctx.w_height, "Respawn");

  SetTargetFPS(ctx.fps); // Set our game to run at 60 frames-per-second

  uint32_t frames = 1;

  enum state old_state = ctx.state;

  menu_setup();

  while (!WindowShouldClose()) {

    if (ctx.state != STATE_MENU_MAIN) {
      handle_click(&ctx);
      handle_message(&ctx);
    }

    if (old_state != ctx.state) {
      printf("Moved from state %d to state %d\n", old_state, ctx.state);
      old_state = ctx.state;
    }

    BeginDrawing();

    ClearBackground(DARKGRAY);

    switch (ctx.state) {
    case STATE_MENU_MAIN: {
      menu_render_main(&ctx.main_menu);

      if (ctx.main_menu.selection == MENU_MAIN_SELECT_LOCAL) {
        ctx.state = STATE_MENU_LOCAL;
      }
      break;
    }
    case STATE_MENU_LOCAL: {
      ctx.local_menu.ready = false;
      ctx.local_menu.back = false;
      menu_render_local(&ctx.local_menu);

      if (ctx.local_menu.back) {
        ctx.state = STATE_MENU_MAIN;
      } else if (ctx.local_menu.ready) {
        ctx.state = STATE_IN_GAME_WAITING;
        init_local(&ctx);
      }
      break;
    }

    case STATE_MAP_READY:
      draw_floor(&ctx);
      draw_portals(&ctx);
      draw_walls(&ctx);
      break;

    case STATE_SELECT_SPAWN:
      draw_floor(&ctx);
      draw_portals(&ctx);
      animation_draw_players(ctx.animation);

      draw_select_square(&ctx, ctx.spawn_opts);
      draw_walls(&ctx);

      if (ctx.selected_square == NULL) {
        draw_title("Select spawn point");
      } else {
        draw_title("Confirm spawn point");
        draw_buttons(&ctx, ctx.confirm_buttons, 2, -1, 20, 5);
      }
      draw_player_stats(&ctx);

      break;

    case STATE_SELECT_SPAWN_FACE:
      draw_floor(&ctx);
      draw_portals(&ctx);
      animation_draw_players(ctx.animation);

      draw_selected_square(&ctx);
      draw_walls(&ctx);

      draw_select_orientation(&ctx);
      if (ctx.facing == DIRECTION_ANY) {
        draw_title("Select orientation");
      } else {
        draw_title("Confirm orientation");
        draw_buttons(&ctx, ctx.confirm_buttons, 2, -1, 20, 5);
      }
      break;

    case STATE_SELECT_MOVE:
      draw_floor(&ctx);
      draw_portals(&ctx);
      draw_health(&ctx);

      animation_draw_players(ctx.animation);
      draw_select_square(&ctx, ctx.move_opts);
      draw_selected_square(&ctx);
      draw_fog_of_war(&ctx);
      draw_walls(&ctx);
      if (ctx.selected_square == NULL) {
        draw_title("Select move");
      } else {
        draw_title("Confirm move");
        draw_buttons(&ctx, ctx.confirm_buttons, 2, -1, 20, 5);
      }

      draw_buttons(&ctx, ctx.spell_buttons, PORTAL_NONE, -1,
                   ctx.margin_top + height * ctx.w_height, 5);
      draw_player_stats(&ctx);

      break;

    case STATE_SELECT_MOVE_FACE:
      draw_floor(&ctx);
      draw_portals(&ctx);
      draw_health(&ctx);

      animation_draw_players(ctx.animation);
      draw_selected_square(&ctx);
      draw_fog_of_war(&ctx);
      draw_walls(&ctx);

      draw_select_orientation(&ctx);

      if (ctx.facing == DIRECTION_ANY) {
        draw_title("Select orientation");
      } else {
        draw_title("Confirm orientations");
        draw_buttons(&ctx, ctx.confirm_buttons, 2, -1, 20, 5);
      }

      draw_buttons(&ctx, ctx.spell_buttons, PORTAL_NONE, -1,
                   ctx.margin_top + height * ctx.w_height, 5);

      break;

    case STATE_SELECT_SPELL:
      draw_floor(&ctx);
      draw_portals(&ctx);
      animation_draw_players(ctx.animation);
      draw_health(&ctx);

      if (ctx.skip_fight) {

        draw_title("Skip fight?");
        draw_buttons(&ctx, ctx.confirm_buttons, 2, -1, 20, 5);
      } else {
        if (ctx.selected_spell != PORTAL_NONE) {
          if (ctx.selected_square == NULL) {
            draw_title("Select target");
          } else {
            draw_title("Confirm target");
            draw_buttons(&ctx, ctx.confirm_buttons, 2, -1, 20, 5);
          }
          draw_select_square(&ctx, ctx.target_opts[ctx.selected_spell]);
        } else {
          draw_title("Select spell");
        }
        draw_selected_square(&ctx);
      }
      draw_fog_of_war(&ctx);
      draw_walls(&ctx);

      draw_buttons(&ctx, ctx.spell_buttons, PORTAL_NONE + 1, -1,
                   ctx.margin_top + height * ctx.w_height, 5);

      draw_attack_info(&ctx);
      draw_player_stats(&ctx);
      break;

    case STATE_IN_GAME_WAITING:
      draw_floor(&ctx);
      draw_portals(&ctx);
      animation_draw_players(ctx.animation);
      draw_fog_of_war(&ctx);
      draw_walls(&ctx);
      draw_health(&ctx);
      draw_player_stats(&ctx);

      draw_buttons(&ctx, ctx.spell_buttons, PORTAL_NONE, -1,
                   ctx.margin_top + height * ctx.w_height, 5);

      break;

    case STATE_ANIMATING:

      if (ctx.animation == NULL) {
        ctx.state = STATE_IN_GAME_WAITING;
        continue;
      }

      draw_floor(&ctx);
      draw_portals(&ctx);
      draw_fog_of_war(&ctx);
      draw_walls(&ctx);
      draw_health(&ctx);
      animation_tick(ctx.animation);

      if (!animation_active(ctx.animation)) {
        ctx.state = STATE_IN_GAME_WAITING;
        animation_clean(ctx.animation);
        handle_waiting_player_update(&ctx);
      }

      break;
    }

    EndDrawing();
    if (ctx.engine != NULL) {
      engine_tick(ctx.engine);
    }
    frames++;
  }
  CloseWindow(); // Close window and OpenGL context

  return 0;
}
