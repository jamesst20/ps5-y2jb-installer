/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef Y2JB_DOWNLOAD0_H
#define Y2JB_DOWNLOAD0_H

#include <stddef.h>

#include "paths.h"
#include "titles.h"

typedef enum {
  DOWNLOAD0_VERSION_UNKNOWN = 0,
  DOWNLOAD0_VERSION_COMPUTING,
  DOWNLOAD0_VERSION_READY,
} download0_version_state_t;

/* Starts a background scan, one title at a time, for each image's embedded version marker:
 * `autoloader_version = "<tag>"` (itsPLK) or `version_string = "<text>"` (Gezine base).
 * A whole-file hash is unstable because YouTube writes into the mounted image. */
void download0_start_refresh(void);

/* Scans path synchronously and stores the result for title_id. Returns 0 when a version
 * was found, 1 when the marker is absent, and -1 when the title is unknown or the file
 * cannot be read. */
int download0_scan_version_at(const char *title_id, const char *path);

/* Copies the title's release string, or "" when unknown. */
void download0_version(const char *title_id, char *out, size_t cap);

/* Returns "unknown", "computing" or "ready". */
const char *download0_version_state_name(const char *title_id);

/* True when the title has a download0.dat. */
int download0_present(const char *title_id);

/* Removes the title's download0.dat and its partial file. A missing file counts as success. */
int download0_remove(const char *title_id);

#endif
