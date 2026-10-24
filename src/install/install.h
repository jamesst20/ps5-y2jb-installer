/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef Y2JB_INSTALL_H
#define Y2JB_INSTALL_H

#define TITLE_DIR_PATH "/user/app/PPSA01650"
#define TITLE_APP_PKG TITLE_DIR_PATH "/app.pkg"
#define TITLE_APP_JSON TITLE_DIR_PATH "/app.json"

typedef enum {
  INSTALL_IDLE,
  INSTALL_RUNNING,
  INSTALL_DONE,
  INSTALL_ERROR,
} install_state_t;

typedef struct {
  install_state_t state;
  char message[256];
} install_status_t;

/* Serves pkg_path on the loopback port and asks the console to install it. expected_digest
 * identifies a same-package reinstall; pass NULL when unknown. delete_when_done removes
 * pkg_path when the job ends. Completion is detected by app.pkg size and app.json content,
 * not timestamps, because Sony preserves the timestamps stored in the PKG. Returns 0 when
 * the worker starts, or -1 when an install is already running. */
int install_start(const char *pkg_path, const char *expected_digest, int delete_when_done);
void install_status(install_status_t *out);

#endif
