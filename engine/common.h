#pragma once

#include <stdint.h>

#define POS_EQ(a, b) (a.x == b.x && a.y == b.y)
#define POS_IS_UNKNOWN(a) (a.x == -1 || a.y == -1)
#define POSITION_UNKNOWN common_position_unknown()
#define PLAYER_UNKNOWN 255

enum portal_type {
  PORTAL_AIR = 0,
  PORTAL_WATER,
  PORTAL_FIRE,
  PORTAL_EARTH,
  PORTAL_NONE
};

enum direction {
  DIRECTION_NORTH = 0,
  DIRECTION_NORTH_EAST,
  DIRECTION_EAST,
  DIRECTION_SOUTH_EAST,
  DIRECTION_SOUTH,
  DIRECTION_SOUTH_WEST,
  DIRECTION_WEST,
  DIRECTION_NORTH_WEST,
  DIRECTION_ANY
};


typedef int32_t coord_t;
typedef struct {
  coord_t x;
  coord_t y;
}pos_t;

pos_t
common_position_unknown(void);

const char* kind_string(enum portal_type type);
