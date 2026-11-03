/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef Y2JB_TITLES_H
#define Y2JB_TITLES_H

#include <stddef.h>

#define TITLE_ID_CAP 16
#define TITLE_COUNT 3

typedef struct {
  const char *region;
  const char *title_id;
} title_entry_t;

/* Every supported YouTube title. The order is stable so per-title state can be indexed by
 * title_index(). */
extern const title_entry_t TITLES[TITLE_COUNT];

/* Returns the title's index, or -1 when the ID is not one of the pinned titles. */
int title_index(const char *title_id);

/* Returns the title entry for a title ID or region name, or NULL when unknown. */
const title_entry_t *title_lookup(const char *title_id);
const title_entry_t *title_by_region(const char *region);

/* Builds one of the title's file paths. Returns 0 for a known title, -1 otherwise or when
 * the output buffer is too small. */
int title_app_dir(const char *title_id, char *out, size_t cap);
int title_app_pkg(const char *title_id, char *out, size_t cap);
int title_app_json(const char *title_id, char *out, size_t cap);
int title_download0_dir(const char *title_id, char *out, size_t cap);
int title_download0_path(const char *title_id, char *out, size_t cap);
int title_download0_partial(const char *title_id, char *out, size_t cap);

/* True when the title's app.pkg exists. */
int title_has_app(const char *title_id);

#endif
