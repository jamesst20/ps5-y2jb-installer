/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "api_util.h"

#include <stdlib.h>
#include <string.h>

#include "cJSON.h"

http_reply_t json_reply(int status, char *json) {
  http_reply_t reply = {status, "application/json", json, json ? strlen(json) : 0};
  if (!json) {
    reply.status = 500;
    reply.content_type = "text/plain; charset=utf-8";
    reply.body = malloc(32);
    if (reply.body) {
      memcpy(reply.body, "out of memory", 14);
      reply.body_len = 13;
    }
  }
  return reply;
}

http_reply_t text_reply(int status, const char *text) {
  http_reply_t reply = {status, "text/plain; charset=utf-8", NULL, 0};
  size_t n = strlen(text);
  reply.body = malloc(n + 1);
  if (reply.body) {
    memcpy(reply.body, text, n + 1);
    reply.body_len = n;
  }
  return reply;
}

http_reply_t result_reply(int status, int ok, const char *message) {
  cJSON *root = cJSON_CreateObject();
  if (!root) {
    return json_reply(500, NULL);
  }
  cJSON_AddBoolToObject(root, "ok", ok);
  cJSON_AddStringToObject(root, "message", message);
  char *json = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  return json_reply(status, json);
}
