/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef Y2JB_DOWNLOAD0_H
#define Y2JB_DOWNLOAD0_H

#include <stddef.h>

#include "paths.h"

#define DOWNLOAD0_DEST "/user/download/PPSA01650/download0.dat"
#define DOWNLOAD0_PARTIAL DOWNLOAD0_DEST ".partial"

typedef enum {
  DOWNLOAD0_VERSION_UNKNOWN = 0,
  DOWNLOAD0_VERSION_COMPUTING,
  DOWNLOAD0_VERSION_READY,
} download0_version_state_t;

/* Starts a background scan for the image's embedded `autoloader_version = "<tag>"` marker.
 * A whole-file hash is unstable because YouTube writes into the mounted image. */
void download0_start_refresh(void);

/* Scans path synchronously. Returns 0 when a version was found, 1 when the marker is
 * absent, and -1 when the file cannot be read. */
int download0_scan_version_at(const char *path);

/* Copies the release string, or "" when unknown. */
void download0_version(char *out, size_t cap);

/* Returns "unknown", "computing" or "ready". */
const char *download0_version_state_name(void);

/* Removes download0.dat and its partial file. A missing file counts as success. */
int download0_remove(void);

#endif
