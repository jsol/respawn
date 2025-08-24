#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "common.h"

typedef struct {
  uint32_t size;
  uint32_t capacity;
  pos_t *data;
} map_opts_t;

map_opts_t *map_opts_new(uint32_t capacity);

map_opts_t *map_opts_clone(map_opts_t *src);
map_opts_t *map_opts_overlap(map_opts_t *a, map_opts_t *b);

bool map_opts_contains(map_opts_t *opts, pos_t needle);

bool map_opts_add(map_opts_t *opts, pos_t id);
bool map_opts_delete(map_opts_t *opts, pos_t id);
void map_opts_delete_list(map_opts_t *opts, map_opts_t *del);
void map_opts_shuffle(map_opts_t *opts);

void map_opts_export(map_opts_t *src, pos_t **data, uint32_t *size);
map_opts_t *map_opts_import(pos_t *data, uint32_t size);

void map_opts_free(map_opts_t *opts);

void map_opts_print(map_opts_t *opts);
