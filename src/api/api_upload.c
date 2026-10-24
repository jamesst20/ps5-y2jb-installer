/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "api_upload.h"

#include <stdio.h>
#include <string.h>

#include "api_util.h"
#include "device/upload.h"

static int query_value(const char *query, const char *key, char *out, size_t cap) {
  size_t key_len = strlen(key);
  for (const char *p = query; *p;) {
    const char *amp = strchr(p, '&');
    size_t len = amp ? (size_t)(amp - p) : strlen(p);
    if (len > key_len && p[key_len] == '=' && strncmp(p, key, key_len) == 0) {
      size_t value_len = len - key_len - 1;
      if (value_len + 1 > cap) {
        return -1;
      }
      memcpy(out, p + key_len + 1, value_len);
      out[value_len] = '\0';
      return 0;
    }
    if (!amp) {
      break;
    }
    p = amp + 1;
  }
  return -1;
}

static int upload_sink_begin(const char *method, const char *url, const char *query, long long length, char *why,
                             size_t cap) {
  (void)method;
  (void)url;

  char slot[16];
  char sha256[65];
  if (query_value(query, "slot", slot, sizeof slot) != 0) {
    snprintf(why, cap, "slot is required");
    return -1;
  }
  if (query_value(query, "sha256", sha256, sizeof sha256) != 0) {
    snprintf(why, cap, "sha256 is required");
    return -1;
  }
  return upload_begin(slot, length, sha256, why, cap);
}

static const http_body_sink_t UPLOAD_SINK = {
    "PUT",
    "/api/upload",
    upload_sink_begin,
    upload_write,
    upload_finish,
    upload_abort,
};

const http_body_sink_t *api_upload_sink(void) {
  return &UPLOAD_SINK;
}

http_reply_t api_upload_cancel(void) {
  if (!upload_running()) {
    return result_reply(409, 0, "No upload is running.");
  }
  upload_abort("cancelled by the app");
  return result_reply(200, 1, "Upload cancelled.");
}
