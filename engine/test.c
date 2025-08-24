#include "common.h"

#include <stdio.h>
int main() {

  pos_t a = {1, 2};
  pos_t b = {3, 4};
  pos_t c = {3, 4};

  a = b;

  printf("A: (%d,%d) %s\n", a.x, a.y, POS_EQ(a, b) ? "Y" : "N");

  return 0;
}
