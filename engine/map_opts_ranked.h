#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "map_opts.h"
#include "common.h"

struct map_opts_rank {
  pos_t pos;
  uint32_t rank;
};

typedef struct {
  uint32_t size;
  uint32_t capacity;
  struct map_opts_rank *data;
} map_opts_ranked_t;

map_opts_ranked_t * map_opts_ranked_new(uint32_t capacity);

bool map_opts_ranked_add(map_opts_ranked_t *opts, pos_t pos, uint32_t rank);

map_opts_t *map_opts_ranked_to_opts(map_opts_ranked_t *opts);

void map_opts_ranked_free(map_opts_ranked_t *opts);

