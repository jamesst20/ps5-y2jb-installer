/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "api_youtube.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "api_util.h"
#include "device/appdb.h"
#include "cJSON.h"
#include "install/install.h"
#include "device/power.h"
#include "device/upload.h"
#include "titles.h"

http_reply_t api_youtube_install(const char *body, size_t body_len) {
  struct stat st;
  if (stat(upload_merged_path(), &st) != 0 || st.st_size <= 0) {
    return result_reply(409, 0, "Upload the merged PKG first.");
  }

  char digest[65] = "";
  char title_id[TITLE_ID_CAP] = "";
  if (body_len > 0) {
    cJSON *root = cJSON_ParseWithLength(body, body_len);
    if (root) {
      const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, "package_digest");
      if (cJSON_IsString(item) && item->valuestring) {
        snprintf(digest, sizeof digest, "%s", item->valuestring);
      }
      const cJSON *title = cJSON_GetObjectItemCaseSensitive(root, "title_id");
      if (cJSON_IsString(title) && title->valuestring) {
        snprintf(title_id, sizeof title_id, "%s", title->valuestring);
      }
      cJSON_Delete(root);
    }
  }

  if (title_index(title_id) < 0) {
    return result_reply(400, 0, "Choose a supported YouTube region first.");
  }

  if (install_start(title_id, upload_merged_path(), digest[0] ? digest : NULL, 1) != 0) {
    /* The app retries requests the console did not answer. If the first one did arrive and
     * started this very install, the retry must not read as a failure. */
    install_status_t running;
    install_status(&running);
    if (running.state == INSTALL_RUNNING && strcmp(running.title_id, title_id) == 0) {
      return result_reply(200, 1, "Installing YouTube.");
    }
    return result_reply(409, 0, "An install is already running.");
  }
  return result_reply(200, 1, "Installing YouTube.");
}

static void add_path_probe(cJSON *array, const char *path) {
  struct stat st;
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "path", path);
  int present = stat(path, &st) == 0;
  cJSON_AddBoolToObject(o, "present", present);
  cJSON_AddNumberToObject(o, "size", (double)(present ? st.st_size : -1));
  cJSON_AddItemToArray(array, o);
}

http_reply_t api_appinfo(const char *body, size_t body_len) {
  char title_id[TITLE_ID_CAP] = "";
  if (body_len > 0) {
    cJSON *root = cJSON_ParseWithLength(body, body_len);
    if (root) {
      const cJSON *title = cJSON_GetObjectItemCaseSensitive(root, "title_id");
      if (cJSON_IsString(title) && title->valuestring) {
        snprintf(title_id, sizeof title_id, "%s", title->valuestring);
      }
      cJSON_Delete(root);
    }
  }
  if (title_index(title_id) < 0) {
    return result_reply(400, 0, "Choose a supported YouTube region first.");
  }
  cJSON *out = cJSON_CreateObject();
  cJSON_AddBoolToObject(out, "ok", 1);
  cJSON_AddStringToObject(out, "title_id", title_id);
  cJSON *rows = appdb_title_rows(title_id);
  if (rows) {
    cJSON_AddItemToObject(out, "rows", rows);
  } else {
    cJSON_AddNullToObject(out, "rows");
  }
  cJSON *probes = cJSON_AddArrayToObject(out, "paths");
  static const char *ROOTS[] = {"/user", "/mnt/ext0/user", "/mnt/ext1/user", "/mnt/usb0/user"};
  for (size_t i = 0; i < sizeof ROOTS / sizeof ROOTS[0]; i++) {
    char path[256];
    snprintf(path, sizeof path, "%s/app/%s/app.pkg", ROOTS[i], title_id);
    add_path_probe(probes, path);
    snprintf(path, sizeof path, "%s/app/%s/app.json", ROOTS[i], title_id);
    add_path_probe(probes, path);
    snprintf(path, sizeof path, "%s/download/%s", ROOTS[i], title_id);
    add_path_probe(probes, path);
  }
  char dl[256];
  if (title_download0_path(title_id, dl, sizeof dl) == 0) {
    add_path_probe(probes, dl);
  }
  char *json = cJSON_PrintUnformatted(out);
  cJSON_Delete(out);
  return json_reply(200, json);
}

http_reply_t api_youtube_block_updates(void) {
  char msg[224];
  if (appdb_block_youtube_updates(msg, sizeof msg) != 0) {
    return result_reply(500, 0, msg);
  }

  if (power_reboot_after(2) != 0) {
    return result_reply(
        200, 1,
        "YouTube updates are blocked, but the restart could not be started. Restart the console yourself.");
  }
  char out[320];
  snprintf(out, sizeof out, "%s The console is restarting.", msg);
  return result_reply(200, 1, out);
}
