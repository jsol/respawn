#include "asset.h"
#include "blink_air.h"

#include "blink_earth.h"

#include "blink_fire.h"

#include "blink_water.h"

#include "cast.h"

#include "dying.h"

#include "growing_ball.h"

#include "move1.h"

#include "move2.h"

#include "sourcerer2.h"

#include "sourcerer.h"

#include "wall.h"

Image asset_get(enum asset ass) {

  Image img = {0};

  switch (ass) {
  case ASSET_BLINK_AIR:

    img.data = BLINK_AIR_DATA;

    img.width = BLINK_AIR_WIDTH;

    img.height = BLINK_AIR_HEIGHT;

    img.format = BLINK_AIR_FORMAT;

    return img;

  case ASSET_BLINK_EARTH:

    img.data = BLINK_EARTH_DATA;

    img.width = BLINK_EARTH_WIDTH;

    img.height = BLINK_EARTH_HEIGHT;

    img.format = BLINK_EARTH_FORMAT;

    return img;

  case ASSET_BLINK_FIRE:

    img.data = BLINK_FIRE_DATA;

    img.width = BLINK_FIRE_WIDTH;

    img.height = BLINK_FIRE_HEIGHT;

    img.format = BLINK_FIRE_FORMAT;

    return img;

  case ASSET_BLINK_WATER:

    img.data = BLINK_WATER_DATA;

    img.width = BLINK_WATER_WIDTH;

    img.height = BLINK_WATER_HEIGHT;

    img.format = BLINK_WATER_FORMAT;

    return img;

  case ASSET_CAST:

    img.data = CAST_DATA;

    img.width = CAST_WIDTH;

    img.height = CAST_HEIGHT;

    img.format = CAST_FORMAT;

    return img;

  case ASSET_DYING:

    img.data = DYING_DATA;

    img.width = DYING_WIDTH;

    img.height = DYING_HEIGHT;

    img.format = DYING_FORMAT;

    return img;

  case ASSET_GROWING_BALL:

    img.data = GROWING_BALL_DATA;

    img.width = GROWING_BALL_WIDTH;

    img.height = GROWING_BALL_HEIGHT;

    img.format = GROWING_BALL_FORMAT;

    return img;

  case ASSET_MOVE1:

    img.data = MOVE1_DATA;

    img.width = MOVE1_WIDTH;

    img.height = MOVE1_HEIGHT;

    img.format = MOVE1_FORMAT;

    return img;

  case ASSET_MOVE2:

    img.data = MOVE2_DATA;

    img.width = MOVE2_WIDTH;

    img.height = MOVE2_HEIGHT;

    img.format = MOVE2_FORMAT;

    return img;

  case ASSET_SOURCERER2:

    img.data = SOURCERER2_DATA;

    img.width = SOURCERER2_WIDTH;

    img.height = SOURCERER2_HEIGHT;

    img.format = SOURCERER2_FORMAT;

    return img;

  case ASSET_SOURCERER:

    img.data = SOURCERER_DATA;

    img.width = SOURCERER_WIDTH;

    img.height = SOURCERER_HEIGHT;

    img.format = SOURCERER_FORMAT;

    return img;

  case ASSET_WALL:

    img.data = WALL_DATA;

    img.width = WALL_WIDTH;

    img.height = WALL_HEIGHT;

    img.format = WALL_FORMAT;

    return img;
  }
  return img;
}
