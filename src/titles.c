/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "titles.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

/* Keep this mapping in sync with the `title_id` fields in pkgs/youtube.json. The Rust
 * manifest tests assert the same IDs. */
const title_entry_t TITLES[TITLE_COUNT] = {
    {"usa", "PPSA01650"},
    {"europe", "PPSA01651"},
    {"japan", "PPSA01652"},
};

int title_index(const char *title_id) {
  if (!title_id) {
    return -1;
  }
  for (int i = 0; i < TITLE_COUNT; i++) {
    if (strcmp(TITLES[i].title_id, title_id) == 0) {
      return i;
    }
  }
  return -1;
}

const title_entry_t *title_lookup(const char *title_id) {
  int index = title_index(title_id);
  return index >= 0 ? &TITLES[index] : NULL;
}

const title_entry_t *title_by_region(const char *region) {
  if (!region) {
    return NULL;
  }
  for (int i = 0; i < TITLE_COUNT; i++) {
    if (strcmp(TITLES[i].region, region) == 0) {
      return &TITLES[i];
    }
  }
  return NULL;
}

static int format_path(const char *title_id, const char *prefix, const char *leaf, char *out,
                       size_t cap) {
  const title_entry_t *title = title_lookup(title_id);
  if (!title || !out || cap == 0) {
    return -1;
  }
  int written = snprintf(out, cap, "%s/%s%s", prefix, title->title_id, leaf);
  return written > 0 && (size_t)written < cap ? 0 : -1;
}

int title_app_dir(const char *title_id, char *out, size_t cap) {
  return format_path(title_id, "/user/app", "", out, cap);
}

int title_app_pkg(const char *title_id, char *out, size_t cap) {
  return format_path(title_id, "/user/app", "/app.pkg", out, cap);
}

int title_app_json(const char *title_id, char *out, size_t cap) {
  return format_path(title_id, "/user/app", "/app.json", out, cap);
}

int title_download0_dir(const char *title_id, char *out, size_t cap) {
  return format_path(title_id, "/user/download", "", out, cap);
}

int title_download0_path(const char *title_id, char *out, size_t cap) {
  return format_path(title_id, "/user/download", "/download0.dat", out, cap);
}

int title_download0_partial(const char *title_id, char *out, size_t cap) {
  return format_path(title_id, "/user/download", "/download0.dat.partial", out, cap);
}

int title_has_app(const char *title_id) {
  char path[256];
  struct stat st;
  return title_app_pkg(title_id, path, sizeof path) == 0 && stat(path, &st) == 0;
}
