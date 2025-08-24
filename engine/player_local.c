#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "message.h"
#include "player_local.h"

struct ctx {
  char tag[4];
  message_t *to_client;
  message_t *to_server;
};

void *player_local_new(void) {
  struct ctx *c;

  c = malloc(sizeof(*c));

  strcpy(c->tag, "PLC");

  c->to_client = NULL;
  c->to_server = NULL;

  return c;
}

void player_local_free(void **data) {
  struct ctx *c = (struct ctx *)(*data);

  if (data == NULL || strcmp(c->tag, "PLC") != 0) {
    return;
  }

  message_unref(c->to_client);
  message_unref(c->to_server);

  free(c);
  *data = NULL;
}

message_t *player_local_get(void *ctx) {
  message_t *tmp;
  struct ctx *c = (struct ctx *)(ctx);

  if (c == NULL) {
    return NULL;
  }
  tmp = c->to_client;

  c->to_client = NULL;

  return tmp;
}

void player_local_send(void *ctx, message_t *msg) {
  struct ctx *c = (struct ctx *)(ctx);

  if (c == NULL) {
    return;
  }

  if (c->to_server != NULL) {
    printf("OVERWRITING CLIENT MESSAGE!!!!!");
    message_unref(c->to_server);
  }

  c->to_server = message_ref(msg);
}

message_t *player_local_server_get(void *ctx) {
  message_t *tmp;
  struct ctx *c = (struct ctx *)(ctx);

  if (c == NULL) {
    return NULL;
  }
  tmp = c->to_server;

  c->to_server = NULL;

  return tmp;
}

void player_local_server_send(void *ctx, message_t *msg) {
  struct ctx *c = (struct ctx *)(ctx);

  if (c == NULL) {
    return;
  }

  if (c->to_client != NULL) {
    printf("OVERWRITING SERVER MESSAGE!!!!!");
    message_unref(c->to_client);
  }

  c->to_client = message_ref(msg);
}
