#pragma once
#include <stdbool.h>
#include <stdint.h>

enum menu_main_selection {
  MENU_MAIN_SELECT_NONE = 0,
  MENU_MAIN_SELECT_LOCAL,
};

struct menu_main {
  enum menu_main_selection selection;
};
struct menu_local {
  bool back;
  bool ready;
  float players;
  float walls;
  int32_t seed;
};

void menu_setup(void);
void menu_render_main(struct menu_main *ctx);
void menu_render_local(struct menu_local *ctx);
