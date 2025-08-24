#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "map.h"
#include "map_opts.h"
#include "map_opts_ranked.h"

#include "message.h"

#define MAP_WALL (1 << 1);
#define MAP_PORTAL (1 << 2);
#define MAP_PLAYER (1 << 3);

struct map_ctx {
  coord_t width;
  coord_t height;
  uint8_t *data;
  map_opts_t *spaces;
  map_opts_t *players;
  map_opts_t *portals;
};

static inline bool in(map_t *ctx, pos_t p) {
  return p.x < ctx->width && p.y < ctx->height && p.x >= 0 && p.y >= 0;
}

static uint32_t to_id(map_t *ctx, pos_t pos) {

  return pos.x * ctx->height + pos.y;
}

static void set_wall(map_t *ctx, pos_t p) {
  if (!in(ctx, p)) {
    return;
  }
  ctx->data[to_id(ctx, p)] |= MAP_WALL;
}

static void unset_wall(map_t *ctx, pos_t p) {
  if (!in(ctx, p)) {
    return;
  }
  ctx->data[to_id(ctx, p)] &= ~MAP_WALL;
}

// NOTE: this is in fact distance^2, but that dont matter when comparing...
static coord_t distance(map_t *ctx, pos_t from, pos_t to) {
  coord_t dist;

  dist =
      ((to.x - from.x) * (to.x - from.x) + (to.y - from.y) * (to.y - from.y));

  return dist;
}

static void build_corridor(map_t *ctx, map_opts_t *curr, pos_t from, pos_t to) {
  if (rand() % 2) {
    if (from.x < to.x) {
      from.x++;
    } else if (from.x > to.x) {
      from.x--;
    } else if (from.y < to.y) {
      from.y++;
    } else if (from.y > to.y) {
      from.y--;
    }
  } else {
    if (from.y < to.y) {
      from.y++;
    } else if (from.y > to.y) {
      from.y--;
    } else if (from.x < to.y) {
      from.x++;
    } else if (from.x > to.y) {
      from.x--;
    }
  }

  if (POS_EQ(from, to)) {
    return;
  }

  map_opts_add(curr, from);
  build_corridor(ctx, curr, from, to);
}

static void create_room(map_t *ctx, pos_t at, uint32_t size) {

  uint32_t tries = 0;
  map_opts_t *curr;

  curr = map_opts_new(size);

  while (curr->size < size) {
    pos_t nr = at;
    uint32_t dir = rand() % 4;

    tries++;
    if (tries > 20) {
      break;
    }

    if (dir == 0) {
      nr.x = at.x - 1;
    } else if (dir == 1) {
      nr.x = at.x + 1;
    } else if (dir == 2) {
      nr.y = at.y - 1;
    } else if (dir == 3) {
      nr.y = at.y + 1;
    }

    if (nr.x >= ctx->width || nr.y >= ctx->height || nr.x < 0 || nr.y < 0 ||
        !map_is_wall(ctx, nr)) {
      continue;
    }

    at = nr;
    map_opts_add(curr, at);
    tries = 0;
  }

  if (ctx->spaces->size > 0) {
    // Draw a corridor the closest way
    coord_t dist = ctx->width * ctx->height;
    pos_t best[2] = {0};

    for (uint32_t i = 0; i < curr->size; i++) {
      for (uint32_t j = 0; j < ctx->spaces->size; j++) {
        uint32_t tmp = distance(ctx, curr->data[i], ctx->spaces->data[j]);

        if (tmp <= 1) {
          // Neighbours
          goto add;
        }

        if (tmp < dist) {
          dist = tmp;
          best[0] = curr->data[i];
          best[1] = ctx->spaces->data[j];
        }
      }
    }

    build_corridor(ctx, curr, best[0], best[1]);
  }

add:

  for (uint32_t i = 0; i < curr->size; i++) {

    unset_wall(ctx, curr->data[i]);
    map_opts_add(ctx->spaces, curr->data[i]);
  }

  map_opts_free(curr);
}

map_t *map_new(coord_t width, coord_t height, int32_t room_factor,
               uint32_t seed) {
  map_t *ctx;

  ctx = malloc(sizeof(*ctx));

  ctx->width = width;
  ctx->height = height;
  ctx->data = calloc(width * height, sizeof(*ctx->data));
  ctx->spaces = map_opts_new(width * height);
  ctx->players = map_opts_new(10);
  ctx->portals = map_opts_new(10);

  for (uint32_t x = 0; x < ctx->width; x++) {
    for (uint32_t y = 0; y < ctx->height; y++) {
      pos_t pos = {x, y};
      set_wall(ctx, pos);
    }
  }

  srand(seed);

  for (coord_t i = width * height / room_factor; i > 0; i--) {
    pos_t p = {0};

    p.x = (rand() % (width - 2)) + 1;
    p.y = (rand() % (height - 2)) + 1;

    if (!map_is_wall(ctx, p)) {
      i++;
      continue;
    }
    create_room(ctx, p, rand() % 20 + 5);
  }

  return ctx;
}

map_t *map_new_from_message(message_t *msg) {
  map_t *ctx;

  ctx = malloc(sizeof(*ctx));
  ctx->width = msg->body.map.width;
  ctx->height = msg->body.map.height;
  ctx->data = malloc(ctx->width * ctx->height * sizeof(*ctx->data));

  memcpy(ctx->data, msg->body.map.data,
         ctx->width * ctx->height * sizeof(*ctx->data));

  ctx->spaces = map_opts_new(ctx->width * ctx->height);
  ctx->players = map_opts_new(msg->body.map.num_players);
  ctx->portals = map_opts_new(msg->body.map.num_portals);

  for (uint8_t i = 0; i < msg->body.map.num_portals; i++) {
    map_opts_add(ctx->portals, msg->body.map.portals[i].pos);
  }

  for (uint32_t y = 0; y < ctx->height; y++) {
    for (uint32_t x = 0; x < ctx->width; x++) {
      pos_t p = {x, y};
      if (!map_is_wall(ctx, p)) {
        map_opts_add(ctx->spaces, p);
      }
    }
  }

  return ctx;
}

coord_t map_height(map_t *ctx) { return ctx->height; }
coord_t map_width(map_t *ctx) { return ctx->width; }

static void valid_moves(map_t *ctx, map_opts_ranked_t *moves, pos_t from,
                        uint8_t steps) {
  pos_t south = from, west = from, east = from, north = from;

  if (steps == 0 || !in(ctx, from) || map_is_wall(ctx, from)) {
    return;
  }

  if (!map_opts_ranked_add(moves, from, steps)) {
    return;
  }

  south.y++;
  north.y--;
  east.x++;
  west.x--;

  valid_moves(ctx, moves, west, steps - 1);
  valid_moves(ctx, moves, east, steps - 1);
  valid_moves(ctx, moves, north, steps - 1);
  valid_moves(ctx, moves, south, steps - 1);
}

map_opts_t *map_valid_moves(map_t *ctx, pos_t from, uint8_t steps) {
  map_opts_ranked_t *opts;
  map_opts_t *ret;

  opts = map_opts_ranked_new(steps * 16);

  valid_moves(ctx, opts, from, steps);

  printf("Ranked size: %u\n", opts->size);

  ret = map_opts_ranked_to_opts(opts);
  map_opts_ranked_free(opts);

  return ret;
}

map_opts_t *map_valid_spawns(map_t *ctx, uint32_t num, uint8_t safe_zone) {

  map_opts_t *opts;
  map_opts_t *ret;

  opts = map_opts_clone(ctx->spaces);

  for (uint32_t i = 0; i < ctx->portals->size; i++) {
    map_opts_t *bad;
    pos_t pos = ctx->portals->data[i];

    bad = map_valid_moves(ctx, pos, safe_zone);
    map_opts_delete_list(opts, bad);
    map_opts_free(bad);
  }

  for (uint32_t i = 0; i < ctx->players->size; i++) {
    map_opts_t *bad;
    map_opts_t *tmp;
    pos_t pos = ctx->players->data[i];

    bad = map_valid_moves(ctx, pos, safe_zone);
    map_opts_delete_list(opts, bad);
    map_opts_free(bad);
    tmp = map_opts_new(opts->size);

    for (uint32_t j = 0; j < opts->size; j++) {
      if (!map_has_los(ctx, pos, opts->data[j])) {
        map_opts_add(tmp, opts->data[j]);
      }
    }

    map_opts_free(opts);
    opts = tmp;
  }

  map_opts_shuffle(opts);

  if (opts->size <= num) {
    return opts;
  }

  ret = map_opts_new(num);

  while (ret->size < num) {
    map_opts_t *bad;
    pos_t pos = opts->data[0];

    map_opts_add(ret, pos);

    bad = map_valid_moves(ctx, pos, safe_zone);
    map_opts_delete_list(opts, bad);
    map_opts_free(bad);

    if (opts->size == 0) {
      break;
    }
  }

  map_opts_free(opts);
  return ret;
}

map_opts_t *map_empty_spaces(map_t *ctx) {
  map_opts_t *opts;

  opts = map_opts_clone(ctx->spaces);

  map_opts_delete_list(opts, ctx->players);
  map_opts_delete_list(opts, ctx->portals);

  return opts;
}

static void los_north(map_t *ctx, map_opts_t *opts, pos_t start) {

  for (coord_t x = 0; x < ctx->width; x++) {
    for (coord_t y = 0; y <= start.y; y++) {
      pos_t p = {x, y};
      if (map_is_wall(ctx, p)) {
        continue;
      }
      if (map_has_los(ctx, start, p)) {
        map_opts_add(opts, p);
      }
    }
  }
}

static void los_south(map_t *ctx, map_opts_t *opts, pos_t start) {
  for (coord_t x = 0; x < ctx->width; x++) {
    for (coord_t y = start.y; y < ctx->height; y++) {
      pos_t p = {x, y};
      if (map_is_wall(ctx, p)) {
        continue;
      }
      if (map_has_los(ctx, start, p)) {
        map_opts_add(opts, p);
      }
    }
  }
}

static void los_west(map_t *ctx, map_opts_t *opts, pos_t start) {
  for (coord_t y = 0; y < ctx->height; y++) {
    for (coord_t x = 0; x <= start.x; x++) {
      pos_t p = {x, y};
      if (map_is_wall(ctx, p)) {
        continue;
      }
      if (map_has_los(ctx, start, p)) {
        map_opts_add(opts, p);
      }
    }
  }
}

static void los_east(map_t *ctx, map_opts_t *opts, pos_t start) {
  for (coord_t y = 0; y < ctx->height; y++) {
    for (coord_t x = start.x; x < ctx->width; x++) {
      pos_t p = {x, y};
      if (map_is_wall(ctx, p)) {
        continue;
      }
      if (map_has_los(ctx, start, p)) {
        map_opts_add(opts, p);
      }
    }
  }
}

static void los_north_west(map_t *ctx, map_opts_t *opts, pos_t start) {
  coord_t max_x = start.x + start.y +1;

  for (coord_t y = 0; y < ctx->height; y++) {
    for (coord_t x = 0; x < ctx->width && x < max_x; x++) {
      pos_t p = {x, y};
      if (map_is_wall(ctx, p)) {
        continue;
      }
      if (map_has_los(ctx, start, p)) {
        map_opts_add(opts, p);
      }
    }

    max_x--;
    if (max_x < 0) {
      return;
    }
  }
}

static void los_north_east(map_t *ctx, map_opts_t *opts, pos_t start) {

  coord_t min_x = start.x - start.y;

  for (coord_t y = 0; y < ctx->height; y++) {
    for (coord_t x = min_x; x < ctx->width; x++) {
      if (x < 0) {
        continue;
      }

      pos_t p = {x, y};
      if (map_is_wall(ctx, p)) {
        continue;
      }
      if (map_has_los(ctx, start, p)) {
        map_opts_add(opts, p);
      }
    }

    min_x++;
    if (min_x >= ctx->width) {
      return;
    }
  }
}

static void los_south_west(map_t *ctx, map_opts_t *opts, pos_t start) {
  coord_t max_x = start.x + (ctx->height - 1 - start.y) +1;

  for (coord_t y = ctx->height - 1; y >= 0; y--) {
    for (coord_t x = 0; x < ctx->width && x < max_x; x++) {
      pos_t p = {x, y};
      if (map_is_wall(ctx, p)) {
        continue;
      }
      if (map_has_los(ctx, start, p)) {
        map_opts_add(opts, p);
      }
    }

    max_x--;
    if (max_x < 0) {
      return;
    }
  }
}
static void los_south_east(map_t *ctx, map_opts_t *opts, pos_t start) {

  coord_t min_x = start.x - (ctx->height - 1 - start.y);

  for (coord_t y = ctx->height - 1; y >= 0; y--) {
    for (coord_t x = min_x; x < ctx->width; x++) {
      if (x < 0) {
        continue;
      }

      pos_t p = {x, y};
      if (map_is_wall(ctx, p)) {
        continue;
      }
      if (map_has_los(ctx, start, p)) {
        map_opts_add(opts, p);
      }
    }

    min_x++;
    if (min_x >= ctx->width) {
      return;
    }
  }
}

map_opts_t *map_line_of_sight(map_t *ctx, pos_t from, enum direction dir) {
  map_opts_t *opts;

  opts = map_opts_new(30);

  switch (dir) {
  case DIRECTION_NORTH:
    los_north(ctx, opts, from);
    break;
  case DIRECTION_SOUTH:
    los_south(ctx, opts, from);
    break;
  case DIRECTION_WEST:
    los_west(ctx, opts, from);
    break;
  case DIRECTION_EAST:
    los_east(ctx, opts, from);
    break;
  case DIRECTION_NORTH_WEST:
    los_north_west(ctx, opts, from);
    break;
  case DIRECTION_NORTH_EAST:
    los_north_east(ctx, opts, from);
    break;
  case DIRECTION_SOUTH_WEST:
    los_south_west(ctx, opts, from);
    break;
  case DIRECTION_SOUTH_EAST:
    los_south_east(ctx, opts, from);
    break;
  case DIRECTION_ANY:
    los_north(ctx, opts, from);
    los_south(ctx, opts, from);
    break;
  default:
    printf("Not implemented");
  }

  return opts;
}

void map_set_portal(map_t *ctx, pos_t pos) {
  if (!in(ctx, pos)) {
    return;
  }
  ctx->data[to_id(ctx, pos)] |= MAP_PORTAL;
  map_opts_add(ctx->portals, pos);
}

void map_unset_portal(map_t *ctx, pos_t pos) {
  if (!in(ctx, pos)) {
    return;
  }
  ctx->data[to_id(ctx, pos)] &= ~MAP_PORTAL;

  map_opts_delete(ctx->portals, pos);
}

void map_set_player(map_t *ctx, pos_t pos) {
  if (!in(ctx, pos)) {
    return;
  }
  ctx->data[to_id(ctx, pos)] |= MAP_PLAYER;
  map_opts_add(ctx->players, pos);
}

void map_unset_player(map_t *ctx, pos_t pos) {
  if (!in(ctx, pos)) {
    return;
  }
  ctx->data[to_id(ctx, pos)] &= ~MAP_PLAYER;

  map_opts_delete(ctx->players, pos);
}

bool map_is_portal(map_t *ctx, pos_t pos) {
  if (!in(ctx, pos)) {
    return false;
  }

  return ctx->data[to_id(ctx, pos)] & MAP_PORTAL;
}

bool map_is_wall(map_t *ctx, pos_t pos) {

  if (!in(ctx, pos)) {
    return false;
  }

  return ctx->data[to_id(ctx, pos)] & MAP_WALL;
}

bool map_is_player(map_t *ctx, pos_t pos) {

  if (!in(ctx, pos)) {
    return false;
  }

  return ctx->data[to_id(ctx, pos)] & MAP_PLAYER;
}

bool map_has_los(map_t *ctx, pos_t from, pos_t to) {
  int32_t dx, dy, sx, sy, err, err2;

  if (map_is_wall(ctx, from) || map_is_wall(ctx, to)) {
    return false;
  }

  if (POS_EQ(to, from)) {
    return true;
  }

  /* Attempts at Bresenham line drawing algorithm */

  dx = abs(to.x - from.x);
  dy = -abs(to.y - from.y);

  sx = from.x < to.x ? 1 : -1;
  sy = from.y < to.y ? 1 : -1;

  err = dx + dy;

  while (true) {
    if (map_is_wall(ctx, from)) {
      return false;
    }
    if (POS_EQ(from, to)) {
      break;
    }

    err2 = err * 2;

    if (err2 >= dy) {
      err += dy;
      from.x += sx;
    }
    if (err2 <= dx) {
      err += dx;
      from.y += sy;
    }
  }

  return true;
}

bool map_within_distance(map_t *ctx, pos_t from, pos_t to, coord_t dist) {

  coord_t dist2 = dist * dist;

  return distance(ctx, from, to) <= dist2;
}

uint32_t map_distance_squared(map_t *ctx, pos_t from, pos_t to) {

  return distance(ctx, from, to);
}

map_opts_t *map_reduce_to_distance(map_t *ctx, pos_t pos, map_opts_t *opts,
                                   coord_t dist) {
  map_opts_t *ret;
  coord_t dist2 = dist * dist;

  printf("Checking against distance %d\n", dist2);
  ret = map_opts_new(opts->size);
  for (uint32_t i = 0; i < opts->size; i++) {
    if (distance(ctx, pos, opts->data[i]) <= dist2) {
      ret->data[ret->size] = opts->data[i];
      ret->size++;
    }
  }

  return ret;
}

message_t *map_to_message(map_t *map, uint32_t tick) {
  message_t *msg;

  msg = message_map(tick);

  msg->body.map.width = map->width;
  msg->body.map.height = map->height;

  msg->body.map.data = malloc(map->width * map->height * sizeof(*map->data));

  memcpy(msg->body.map.data, map->data,
         map->width * map->height * sizeof(*map->data));

  return msg;
}

bool map_valid_direction(enum direction dir) {

  return dir >= 0 && dir < DIRECTION_ANY;
}

map_opts_t *map_players(map_t *ctx, map_opts_t *at) {

  if (at == NULL) {
    return map_opts_clone(ctx->players);
  }
  return map_opts_overlap(ctx->players, at);
}

map_opts_t *map_portals(map_t *ctx, map_opts_t *at) {
  if (at == NULL) {
    return map_opts_clone(ctx->portals);
  }
  return map_opts_overlap(ctx->portals, at);
}
