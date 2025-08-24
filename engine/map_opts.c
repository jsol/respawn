#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "map_opts.h"

map_opts_t *map_opts_new(uint32_t capacity) {
  map_opts_t *ctx;

  ctx = malloc(sizeof(*ctx));
  ctx->size = 0;
  ctx->capacity = capacity;
  ctx->data = calloc(capacity, sizeof(*ctx->data));

  return ctx;
}

map_opts_t *map_opts_clone(map_opts_t *src) {
  map_opts_t *ctx;

  ctx = malloc(sizeof(*ctx));
  ctx->size = src->size;
  ctx->capacity = src->capacity;
  ctx->data = calloc(ctx->capacity, sizeof(*ctx->data));

  memcpy(ctx->data, src->data, src->size * sizeof(*src->data));

  return ctx;
}

bool map_opts_contains(map_opts_t *opts, pos_t needle) {
  if (opts == NULL) {
    return false;
  }

  for (uint32_t i = 0; i < opts->size; i++) {
    if (POS_EQ(opts->data[i], needle)) {
      return true;
    }
  }

  return false;
}

bool map_opts_add(map_opts_t *opts, pos_t pos) {

  if (map_opts_contains(opts, pos)) {
    return false;
  }

  if (opts->size == opts->capacity) {
    opts->capacity = opts->capacity * 2;
    // TODO: Fix NULL return
    opts->data = realloc(opts->data, sizeof(*opts->data) * opts->capacity);
  }

  opts->data[opts->size] = pos;
  opts->size++;

  return true;
}

bool map_opts_delete(map_opts_t *opts, pos_t pos) {

  for (uint32_t i = 0; i < opts->size; i++) {
    if (POS_EQ(opts->data[i], pos)) {
      opts->size--;
      opts->data[i] = opts->data[opts->size];
      return true;
    }
  }

  return false;
}

void map_opts_delete_list(map_opts_t *opts, map_opts_t *del) {
  for (uint32_t i = 0; i < del->size; i++) {
    map_opts_delete(opts, del->data[i]);
  }
}

static void swap(map_opts_t *opts, uint32_t a, uint32_t b) {
  pos_t tmp;

  tmp = opts->data[a];
  opts->data[a] = opts->data[b];
  opts->data[b] = tmp;
}

void map_opts_shuffle(map_opts_t *opts) {

  for (uint32_t i = 0; i < opts->size; i++) {
    swap(opts, i, rand() % opts->size);
  }
}

map_opts_t *map_opts_overlap(map_opts_t *a, map_opts_t *b) {
  map_opts_t *big;
  map_opts_t *small;
  map_opts_t *ret;

  if (a->size > b->size) {
    big = a;
    small = b;
  } else {
    big = b;
    small = a;
  }

  ret = map_opts_new(small->size);
  for (uint32_t i = 0; i < small->size; i++) {
    if (map_opts_contains(big, small->data[i])) {
      ret->data[ret->size] = small->data[i];
      ret->size++;
    }
  }
  return ret;
}

void map_opts_export(map_opts_t *src, pos_t **data, uint32_t *size) {
  *size = src->size;
  if (src->size == 0) {
    *data = NULL;
  } else {
    *data = malloc(src->size * sizeof(*src->data));

    memcpy(*data, src->data, src->size * sizeof(*src->data));
  }
}

map_opts_t *map_opts_import(pos_t *data, uint32_t size) {
  map_opts_t *tmp;

  if (size == 0) {
    return map_opts_new(1);
  } else {
    tmp = map_opts_new(size);
    tmp->size = size;
    memcpy(tmp->data, data, size * sizeof(*tmp->data));
  }

  return tmp;
}

void map_opts_free(map_opts_t *opts) {

  if (opts == NULL) {
    return;
  }
  free(opts->data);
  free(opts);
}

void map_opts_print(map_opts_t *opts) {
  printf("Opts (%u / %u):", opts->size, opts->capacity);
  for (uint32_t i = 0; i < opts->size; i++) {
    printf(" (%d,%d)", opts->data[i].x, opts->data[i].y);
  }
  printf("\n");
}
