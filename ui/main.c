
#include <math.h>
#include <raylib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "engine.h"
#include "map.h"
#include "map_opts.h"
#include "message.h"
#include "player.h"
#include "player_local.h"
#include "player_npc.h"
#include "portals.h"

#define SCREEN_WIDTH 1080
#define SCREEN_HEIGHT 768

#define BUTTON_ID_OK 20
#define BUTTON_ID_NOPE 21

#define SELECT_FACING_RADIUS 200

#define NUM_PLAYERS 6
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
  STATE_SELECT_FACING,
  STATE_IN_GAME_WAITING
};

typedef struct {

  uint8_t id;
  int32_t padding;
  int32_t text_width;

  const char *text;
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
  Texture2D *player_images;
  uint8_t player_count;

  button_t spell_buttons[PORTAL_NONE + 1];
  button_t confirm_buttons[2];

  Texture2D walls;
  Texture2D floor;
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
    DrawCircle(map_x(ctx, pos.x) + ctx->w_width / 2,
               map_y(ctx, pos.y) + ctx->w_height / 2, 10,
               portal_color(p->kind));
  }
}

static Color player_color(player_t *p) {
  switch (p->id) {
  case 0:
    return MAGENTA;
  case 1:
    return DARKGREEN;
  case 2:
    return MAROON;
  case 3:
    return BEIGE;
  case 4:
    return LIME;
  case 5:
    return ORANGE;
  default:
    return RED;
  }

  return RED;
}

static void setup_spell_buttons(ctx_t *ctx) {

  if (ctx->me == NULL) {
    return;
  }
  for (uint8_t i = 0; i < PORTAL_NONE; i++) {
    ctx->spell_buttons[i].id = i;
    ctx->spell_buttons[i].font_size = 16;
    ctx->spell_buttons[i].padding = 4;
    ctx->spell_buttons[i].visible = true;

    if (ctx->me->spells[i] != NULL) {
      ctx->spell_buttons[i].text = ctx->me->spells[i]->name;
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
  ctx->spell_buttons[PORTAL_NONE].font_size = 16;
  ctx->spell_buttons[PORTAL_NONE].padding = 4;
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

static void show_confirm(ctx_t *ctx) {
  ctx->confirm_buttons[0].visible = true;
  ctx->confirm_buttons[1].visible = true;
}
static void hide_confirm(ctx_t *ctx) {
  ctx->confirm_buttons[0].visible = false;
  ctx->confirm_buttons[1].visible = false;
}

static void draw_buttons(ctx_t *ctx, button_t *buttons, uint8_t num_buttons,
                         int32_t y, int32_t spacing) {

  int32_t total_width = 0;
  int32_t x = 0;

  for (uint8_t i = 0; i < num_buttons; i++) {
    total_width += buttons[i].text_width + buttons[i].padding * 2 + spacing;
  }

  total_width -= spacing;

  x = GetScreenWidth() / 2 - total_width / 2;

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
    DrawText(buttons[i].text, x + buttons[i].padding, y + buttons[i].padding,
             buttons[i].font_size, BLACK);
    x += buttons[i].padding + spacing + buttons[i].text_width;
  }
}

static void setup_players(ctx_t *ctx) {
  Image orig;

  if (ctx->player_images != NULL) {
    return;
  }

  ctx->player_images = malloc(ctx->player_count * sizeof(*ctx->player_images));

  orig = LoadImage("/home/jens/git/respawn/assets/sourcerer.png");

  for (uint8_t i = 0; i < ctx->player_count; i++) {
    Image img;

    img = ImageCopy(orig);
    ImageColorReplace(&img, RED, player_color(&ctx->players[i]));

    ctx->player_images[i] = LoadTextureFromImage(img);
    UnloadImage(img);
  }
  UnloadImage(orig);
}

static void draw_players(ctx_t *ctx) {
  for (uint32_t i = 0; i < NUM_PLAYERS; i++) {
    player_t *p = &ctx->players[i];
    if (p->health == 0) {
      continue;
    }

    Vector2 pos = {.x = map_x(ctx, p->position.x),
                   .y = map_y(ctx, p->position.y)};

    switch (p->facing) {
    case DIRECTION_NORTH:
      break;
    case DIRECTION_NORTH_EAST:
      pos.x += ctx->w_width / 2;
      break;
    case DIRECTION_EAST:
      pos.x += ctx->w_width;
      break;
    case DIRECTION_SOUTH_EAST:
      pos.y += ctx->w_height / 2;
      pos.x += ctx->w_width + ctx->w_width / 2;
      break;
    case DIRECTION_SOUTH:
      pos.y += ctx->w_height;
      pos.x += ctx->w_width;
      break;
    case DIRECTION_SOUTH_WEST:
      pos.y += ctx->w_height;
      pos.x += ctx->w_width / 2;
      break;
    case DIRECTION_WEST:
      pos.y += ctx->w_height;
      break;
    case DIRECTION_NORTH_WEST:
      pos.y += ctx->w_height / 2;
      break;

    default:
      break;
    }

    DrawTextureEx(ctx->player_images[i], pos, p->facing * 45, 1.0f,
                  player_color(p));
  }
}

static void init_local(ctx_t *ctx) {
  uint32_t width = 80;
  uint32_t height = 40;
  map_t *map = map_new(width, height, 30, 7);

  ctx->msg_ctx = player_local_new();
  ctx->send_msg = player_local_send;
  ctx->get_msg = player_local_get;

  ctx->engine = engine_new(NUM_PLAYERS, map, NULL, false);

  engine_add_player(ctx->engine, player_local_server_send, ctx->msg_ctx,
                    player_local_server_get, ctx->msg_ctx);

  for (uint8_t i = 1; i < NUM_PLAYERS; i++) {
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

static void setup_walls(ctx_t *ctx) {
  Image img;
  coord_t height = map_height(ctx->map);
  coord_t width = map_width(ctx->map);

  img = GenImageColor(width * ctx->w_width, height * ctx->w_height, BLANK);

  for (coord_t y = 0; y < height; y++) {
    for (coord_t x = 0; x < width; x++) {
      pos_t p = {x, y};
      if (map_is_wall(ctx->map, p)) {
        ImageDrawRectangle(&img, x * ctx->w_width, y * ctx->w_height,
                           ctx->w_width, ctx->w_height, BLACK);
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

  switch (msg->type) {
  case MESSAGE_ASK_READY:
    reply = message_reply_ready(msg->tick);
    break;

  case MESSAGE_MAP:
    ctx->map = map_new_from_message(msg);
    ctx->portals = portals_new_from_message(msg);
    ctx->player_count = msg->body.map.num_players;
    ctx->players = player_create(ctx->player_count);
    setup_players(ctx);
    setup_floor(ctx);
    setup_walls(ctx);

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

  case MESSAGE_PLAYER_UPDATE: {
    player_batch_update(ctx->players, ctx->player_count, msg);
    portals_update(ctx->portals, msg);

    setup_spell_buttons(ctx);

    map_opts_free(ctx->los_opts);
    ctx->los_opts = map_opts_import(msg->body.player_update.los.opts,
                                    msg->body.player_update.los.size);

    ctx->skip_fight = msg->body.player_update.num_others == 0;
    if (ctx->skip_fight) {
      show_confirm(ctx);
    }
    printf("Updated player:");
    printf("Facing: %u\n", ctx->me->facing);
    map_opts_print(ctx->los_opts);

    reply = message_reply_player_update(msg->tick);
    break;
  }

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

          reply = message_reply_fight(ctx->reply_tick, PORTAL_NONE,
                                      POSITION_UNKNOWN);
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

          reply = message_reply_fight(ctx->reply_tick, ctx->selected_spell,
                                      *ctx->selected_square);
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

  ctx.state = STATE_START_LOCAL;
  init_local(&ctx);

  InitWindow(ctx.margin_left * 2 + width * ctx.w_width,
             ctx.margin_top * 2 + height * ctx.w_height, "Respawn");

  SetTargetFPS(ctx.fps); // Set our game to run at 60 frames-per-second

  uint32_t frames = 0;

  enum state old_state = ctx.state;

  while (!WindowShouldClose()) {
    handle_click(&ctx);
    handle_message(&ctx);

    if (old_state != ctx.state) {
      printf("Moved from state %d to state %d\n", old_state, ctx.state);
      old_state = ctx.state;
    }

    BeginDrawing();

    ClearBackground(DARKGRAY);

    switch (ctx.state) {
    case STATE_MAP_READY:
      draw_floor(&ctx);
      draw_portals(&ctx);
      draw_walls(&ctx);
      break;

    case STATE_SELECT_SPAWN:
      draw_floor(&ctx);
      draw_portals(&ctx);

      draw_select_square(&ctx, ctx.spawn_opts);
      draw_walls(&ctx);

      if (ctx.selected_square == NULL) {
        draw_title("Select spawn point");
      } else {
        draw_title("Confirm spawn point");
        draw_buttons(&ctx, ctx.confirm_buttons, 2, 20, 5);
      }

      break;

    case STATE_SELECT_SPAWN_FACE:
      draw_floor(&ctx);
      draw_portals(&ctx);

      draw_selected_square(&ctx);
      draw_walls(&ctx);

      draw_select_orientation(&ctx);
      if (ctx.facing == DIRECTION_ANY) {
        draw_title("Select orientation");
      } else {
        draw_title("Confirm orientation");
        draw_buttons(&ctx, ctx.confirm_buttons, 2, 20, 5);
      }
      break;

    case STATE_SELECT_MOVE:
      draw_floor(&ctx);
      draw_portals(&ctx);

      draw_players(&ctx);
      draw_select_square(&ctx, ctx.move_opts);
      draw_selected_square(&ctx);
      draw_fog_of_war(&ctx);
      draw_walls(&ctx);
      if (ctx.selected_square == NULL) {
        draw_title("Select move");
      } else {
        draw_title("Confirm move");
        draw_buttons(&ctx, ctx.confirm_buttons, 2, 20, 5);
      }

      draw_buttons(&ctx, ctx.spell_buttons, PORTAL_NONE,
                   ctx.margin_top + height * ctx.w_height, 5);

      break;

    case STATE_SELECT_MOVE_FACE:
      draw_floor(&ctx);
      draw_portals(&ctx);

      draw_players(&ctx);
      draw_selected_square(&ctx);
      draw_fog_of_war(&ctx);
      draw_walls(&ctx);

      draw_select_orientation(&ctx);

      if (ctx.facing == DIRECTION_ANY) {
        draw_title("Select orientation");
      } else {
        draw_title("Confirm orientations");
        draw_buttons(&ctx, ctx.confirm_buttons, 2, 20, 5);
      }

      draw_buttons(&ctx, ctx.spell_buttons, PORTAL_NONE,
                   ctx.margin_top + height * ctx.w_height, 5);

      break;

    case STATE_SELECT_SPELL:
      draw_floor(&ctx);
      draw_portals(&ctx);
      draw_players(&ctx);

      if (ctx.skip_fight) {

        draw_title("Skip fight?");
        draw_buttons(&ctx, ctx.confirm_buttons, 2, 20, 5);
      } else {
        if (ctx.selected_spell != PORTAL_NONE) {
          if (ctx.selected_square == NULL) {
            draw_title("Select target");
          } else {
            draw_title("Confirm target");
            draw_buttons(&ctx, ctx.confirm_buttons, 2, 20, 5);
          }
          draw_select_square(&ctx, ctx.target_opts[ctx.selected_spell]);
        } else {
          draw_title("Select spell");
        }
        draw_selected_square(&ctx);
      }
      draw_fog_of_war(&ctx);
      draw_walls(&ctx);

      draw_buttons(&ctx, ctx.spell_buttons, PORTAL_NONE + 1,
                   ctx.margin_top + height * ctx.w_height, 5);

      break;

    case STATE_IN_GAME_WAITING:
      draw_floor(&ctx);
      draw_portals(&ctx);
      draw_players(&ctx);
      draw_fog_of_war(&ctx);
      draw_walls(&ctx);

      draw_buttons(&ctx, ctx.spell_buttons, PORTAL_NONE,
                   ctx.margin_top + height * ctx.w_height, 5);

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
