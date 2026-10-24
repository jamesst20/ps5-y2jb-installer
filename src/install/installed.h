/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef Y2JB_INSTALLED_H
#define Y2JB_INSTALLED_H

#include <stddef.h>

typedef struct {
  int installed;             /* app.pkg exists */
  long long pkg_size;        /* -1 when absent */
  char version[32];          /* version string from app.json */
  char app_json_sha256[65];  /* SHA-256 of app.json */
  char package_digest[65];   /* uppercase hex from app.json */
  char excerpt[512];         /* first bytes of app.json */
} installed_info_t;

/* Reads the YouTube title directory. Absent fields are "" or -1. */
void installed_read(installed_info_t *out);

#endif
