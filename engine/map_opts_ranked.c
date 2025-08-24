#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "map_opts.h"
#include "map_opts_ranked.h"

map_opts_ranked_t *map_opts_ranked_new(uint32_t capacity) {
  map_opts_ranked_t *ctx;

  ctx = malloc(sizeof(*ctx));
  ctx->size = 0;
  ctx->capacity = capacity;
  ctx->data = calloc(capacity, sizeof(*ctx->data));

  return ctx;
}

bool map_opts_ranked_add(map_opts_ranked_t *opts, pos_t pos, uint32_t rank) {

  for (uint32_t i = 0; i < opts->size; i++) {
    if (POS_EQ(opts->data[i].pos, pos)) {

      if (opts->data[i].rank < rank) {
        opts->data[i].rank = rank;
        return true;
      }

      return false;
    }
  }

  if (opts->size == opts->capacity) {
    opts->capacity = opts->capacity * 2;
    // TODO: Fix NULL return
    opts->data = realloc(opts->data, sizeof(*opts->data) * opts->capacity);
  }

  opts->data[opts->size].pos = pos;
  opts->data[opts->size].rank = rank;
  opts->size++;
  return true;
}

map_opts_t *map_opts_ranked_to_opts(map_opts_ranked_t *opts) {
  map_opts_t *ret;

  ret = map_opts_new(opts->size);

  for (uint32_t i = 0; i < opts->size; i++) {
    ret->data[i] = opts->data[i].pos;
  }
  ret->size = opts->size;

  return ret;
}

void map_opts_ranked_free(map_opts_ranked_t *opts) {

  if (opts == NULL) {
    return;
  }
  free(opts->data);
  free(opts);
}
