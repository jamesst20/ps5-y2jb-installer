/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "api_download0.h"

#include <stdio.h>
#include <string.h>

#include "api_util.h"
#include "cJSON.h"
#include "device/download0.h"
#include "device/upload.h"
#include "titles.h"

http_reply_t api_download0_remove(const char *body, size_t body_len) {
  char title_id[TITLE_ID_CAP] = "";
  if (body_len > 0) {
    cJSON *root = cJSON_ParseWithLength(body, body_len);
    if (root) {
      const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, "title_id");
      if (cJSON_IsString(item) && item->valuestring) {
        snprintf(title_id, sizeof title_id, "%s", item->valuestring);
      }
      cJSON_Delete(root);
    }
  }
  if (title_index(title_id) < 0) {
    return result_reply(400, 0, "Choose a supported YouTube region first.");
  }

  upload_status_t st;
  upload_status(&st);
  if (st.state == UPLOAD_RUNNING && strcmp(st.slot, UPLOAD_SLOT_DOWNLOAD0) == 0) {
    return result_reply(409, 0, "The download0.dat upload is still running.");
  }
  if (download0_remove(title_id) != 0) {
    return result_reply(500, 0, "Could not remove download0.dat from the YouTube folder.");
  }
  return result_reply(200, 1, "Removed download0.dat from that YouTube folder. Y2JB is not set up for that region until it is uploaded again.");
}
