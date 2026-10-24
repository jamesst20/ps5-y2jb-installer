/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "api_youtube.h"

#include <stdio.h>
#include <sys/stat.h>

#include "api_util.h"
#include "device/appdb.h"
#include "cJSON.h"
#include "install/install.h"
#include "device/power.h"
#include "device/upload.h"

http_reply_t api_youtube_install(const char *body, size_t body_len) {
  struct stat st;
  if (stat(upload_merged_path(), &st) != 0 || st.st_size <= 0) {
    return result_reply(409, 0, "Upload the merged PKG first.");
  }

  char digest[65] = "";
  if (body_len > 0) {
    cJSON *root = cJSON_ParseWithLength(body, body_len);
    if (root) {
      const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, "package_digest");
      if (cJSON_IsString(item) && item->valuestring) {
        snprintf(digest, sizeof digest, "%s", item->valuestring);
      }
      cJSON_Delete(root);
    }
  }

  if (install_start(upload_merged_path(), digest[0] ? digest : NULL, 1) != 0) {
    return result_reply(409, 0, "An install is already running.");
  }
  return result_reply(200, 1, "Installing YouTube.");
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
