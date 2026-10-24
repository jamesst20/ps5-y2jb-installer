/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "installed.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "cJSON.h"
#include "install.h"
#include "crypto/sha256.h"

#define JSON_CAP (1024 * 1024)

static long long size_of(const char *path) {
  struct stat st;
  return stat(path, &st) == 0 ? (long long)st.st_size : -1;
}

static const char *find_version(const cJSON *node) {
  static const char *keys[] = {"app_ver", "APP_VER", "app_version", "APP_VERSION", "version", "VERSION",
                               "contentVersion", "content_version"};
  if (!node) {
    return NULL;
  }
  if (cJSON_IsObject(node)) {
    for (size_t k = 0; k < sizeof keys / sizeof keys[0]; k++) {
      const cJSON *item = cJSON_GetObjectItemCaseSensitive(node, keys[k]);
      if (cJSON_IsString(item) && item->valuestring) {
        return item->valuestring;
      }
    }
  }
  const cJSON *child = NULL;
  cJSON_ArrayForEach(child, node) {
    const char *found = find_version(child);
    if (found) {
      return found;
    }
  }
  return NULL;
}

void installed_read(installed_info_t *out) {
  memset(out, 0, sizeof *out);
  out->pkg_size = size_of(TITLE_APP_PKG);
  out->installed = out->pkg_size >= 0;
  if (!out->installed) {
    return;
  }
  if (size_of(TITLE_APP_JSON) < 0) {
    return;
  }
  compute_sha256_file(TITLE_APP_JSON, out->app_json_sha256);

  FILE *f = fopen(TITLE_APP_JSON, "rb");
  if (!f) {
    return;
  }
  char *text = malloc(JSON_CAP);
  size_t n = text ? fread(text, 1, JSON_CAP - 1, f) : 0;
  fclose(f);
  if (!text) {
    return;
  }
  text[n] = '\0';
  snprintf(out->excerpt, sizeof out->excerpt, "%.*s", (int)(n < 480 ? n : 480), text);
  cJSON *root = cJSON_Parse(text);
  const char *version = find_version(root);
  if (version) {
    snprintf(out->version, sizeof out->version, "%s", version);
  }
  const cJSON *digest = cJSON_GetObjectItemCaseSensitive(root, "packageDigest");
  if (cJSON_IsString(digest) && digest->valuestring) {
    snprintf(out->package_digest, sizeof out->package_digest, "%s", digest->valuestring);
  }
  cJSON_Delete(root);
  free(text);
}
