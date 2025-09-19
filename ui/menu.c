#define RAYGUI_IMPLEMENTATION
#include "menu.h"
#include "raygui.h"

void menu_setup(void) { GuiSetStyle(DEFAULT, TEXT_SIZE, 20); }

void menu_render_main(struct menu_main *ctx) {

  if (GuiButton((Rectangle){24, 24, 120, 30}, "Local game")) {
    ctx->selection = MENU_MAIN_SELECT_LOCAL;
    return;
  }

  ctx->selection = MENU_MAIN_SELECT_NONE;
}

void menu_render_local(struct menu_local *ctx) {
  char players[32];
  static char seed[32];
  char walls[32];
  static bool edit_mode = false;

  if (GuiButton((Rectangle){24, 24, 120, 30}, "Back")) {
    ctx->back = true;
    return;
  }
  if (GuiButton((Rectangle){24, 54, 120, 30}, "Start")) {
    ctx->ready = true;
    return;
  }

  sprintf(players, "Players: %d", (int)ctx->players);
  sprintf(walls, "Close-ness: %d%%", (int)ctx->walls);
  GuiLabel((Rectangle){24, 84, 120, 30}, players);
  GuiSlider((Rectangle){144, 84, 240, 30}, "", "", &ctx->players, 4.0, 16.0);

  GuiLabel((Rectangle){24, 114, 120, 30}, "Random seed:");
  if (GuiTextBox((Rectangle){144, 114, 240, 30}, seed, 20, edit_mode)) {
    if (edit_mode) {
      ctx->seed = atoi(seed);
      snprintf(seed, 20, "%d", ctx->seed);
      printf("Seed: %s ->%d\n", seed, ctx->seed);
    }
    edit_mode = !edit_mode;
  }

  GuiLabel((Rectangle){24, 144, 120, 30}, walls);
  GuiSlider((Rectangle){144, 144, 240, 30}, "", "", &ctx->walls, 15.0, 40.0);
}
