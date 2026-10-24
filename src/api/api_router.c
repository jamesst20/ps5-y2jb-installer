/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "api_router.h"

#include <string.h>

#include "api_accounts.h"
#include "api_download0.h"
#include "api_power.h"
#include "api_upload.h"
#include "api_util.h"
#include "api_youtube.h"
#include "cJSON.h"
#include "api_status.h"
#include "version.h"

static http_reply_t banner(void) {
  cJSON *root = cJSON_CreateObject();
  if (!root) {
    return json_reply(500, NULL);
  }
  cJSON_AddBoolToObject(root, "ok", 1);
  cJSON_AddStringToObject(root, "service", "y2jb-installer");
  cJSON_AddStringToObject(root, "version", Y2JB_VERSION);
  char *json = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  return json_reply(200, json);
}

http_reply_t api_route(const char *method, const char *path, const char *body, size_t body_len) {
  int get = strcmp(method, "GET") == 0;
  int post = strcmp(method, "POST") == 0;

  if (get && strcmp(path, "/") == 0) {
    return banner();
  }
  if (get && strcmp(path, "/api/status") == 0) {
    return api_status();
  }
  if (post && strcmp(path, "/api/upload/cancel") == 0) {
    return api_upload_cancel();
  }
  if (post && strcmp(path, "/api/youtube/install") == 0) {
    return api_youtube_install(body, body_len);
  }
  if (post && strcmp(path, "/api/youtube/block-updates") == 0) {
    return api_youtube_block_updates();
  }
  if (post && strcmp(path, "/api/download0/remove") == 0) {
    return api_download0_remove();
  }
  if (post && strcmp(path, "/api/activate") == 0) {
    return api_activate(body, body_len);
  }
  if (post && strcmp(path, "/api/fakesignin") == 0) {
    return api_fakesignin(body, body_len);
  }
  if (post && strcmp(path, "/api/reboot") == 0) {
    return api_reboot();
  }
  if (post && strcmp(path, "/api/shutdown") == 0) {
    return api_shutdown();
  }
  if (strncmp(path, "/api/", 5) == 0) {
    return text_reply(404, "not found");
  }
  if (!get) {
    return text_reply(405, "method not allowed");
  }
  return text_reply(404, "not found");
}
