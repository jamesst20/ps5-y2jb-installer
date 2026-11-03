/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef Y2JB_INSTALL_H
#define Y2JB_INSTALL_H

#include "titles.h"

typedef enum {
  INSTALL_IDLE,
  INSTALL_RUNNING,
  INSTALL_DONE,
  INSTALL_ERROR,
} install_state_t;

typedef struct {
  install_state_t state;
  char title_id[TITLE_ID_CAP];
  char message[256];
  char console_status[24];
  unsigned long long downloaded_bytes;
  unsigned long long total_bytes;
  int progress; /* 0-100, or -1 when the console does not report progress */
  int error_code;
  char error_description[512];
} install_status_t;

/* Serves pkg_path on the loopback port and asks the console to install it into title_id's
 * folder. expected_digest identifies a same-package reinstall; pass NULL when unknown.
 * delete_when_done removes pkg_path when the job ends. Completion is detected by app.pkg
 * size and app.json content, not timestamps, because Sony preserves the timestamps stored
 * in the PKG. Returns 0 when the worker starts, or -1 when an install is already running or
 * the title ID is unknown. */
int install_start(const char *title_id, const char *pkg_path, const char *expected_digest,
                  int delete_when_done);
void install_status(install_status_t *out);

#endif
