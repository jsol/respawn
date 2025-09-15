#include "common.h"

pos_t common_position_unknown(void) {
  pos_t p = {-1, -1};
  return p;
}
const char *kind_string(enum portal_type type) {

  switch (type) {

  case PORTAL_AIR:
    return "Air";
  case PORTAL_WATER:
    return "Water";
  case PORTAL_EARTH:
    return "Earth";
  case PORTAL_FIRE:
    return "Fire";
  case PORTAL_NONE:
    return "Unknown";
  }
  return "Unknown";
}
