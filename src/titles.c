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

static title_app_root_fn g_app_root_resolver = NULL;

void title_set_app_root_resolver(title_app_root_fn resolver) {
  g_app_root_resolver = resolver;
}

/* The internal storage first, then the M.2 SSD and other extended-storage mounts. A PS5
 * whose default install location is the M.2 SSD puts apps under /mnt/ext1. */
static const char *const APP_ROOTS[] = {"/user/app", "/mnt/ext1/user/app", "/mnt/ext0/user/app"};

static int app_dir_has_pkg(const char *dir) {
  char pkg[512];
  struct stat st;
  return snprintf(pkg, sizeof pkg, "%s/app.pkg", dir) < (int)sizeof pkg && stat(pkg, &st) == 0;
}

/* Writes the title's install directory: the resolver's answer, else the first known root
 * holding an app.pkg, else the internal-storage path (so callers that create or report a
 * path still get the conventional one). */
static int app_dir(const char *title_id, char *out, size_t cap) {
  if (!title_lookup(title_id) || !out || cap == 0) {
    return -1;
  }
  if (g_app_root_resolver && g_app_root_resolver(title_id, out, cap) == 0 && app_dir_has_pkg(out)) {
    return 0;
  }
  for (size_t i = 0; i < sizeof APP_ROOTS / sizeof APP_ROOTS[0]; i++) {
    if (format_path(title_id, APP_ROOTS[i], "", out, cap) == 0 && app_dir_has_pkg(out)) {
      return 0;
    }
  }
  return format_path(title_id, APP_ROOTS[0], "", out, cap);
}

static int app_path(const char *title_id, const char *leaf, char *out, size_t cap) {
  char dir[512];
  if (app_dir(title_id, dir, sizeof dir) != 0 || !out || cap == 0) {
    return -1;
  }
  int written = snprintf(out, cap, "%s%s", dir, leaf);
  return written > 0 && (size_t)written < cap ? 0 : -1;
}

int title_app_dir(const char *title_id, char *out, size_t cap) {
  return app_path(title_id, "", out, cap);
}

int title_app_pkg(const char *title_id, char *out, size_t cap) {
  return app_path(title_id, "/app.pkg", out, cap);
}

int title_app_json(const char *title_id, char *out, size_t cap) {
  return app_path(title_id, "/app.json", out, cap);
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
