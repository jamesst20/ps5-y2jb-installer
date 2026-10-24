/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef Y2JB_UPLOAD_H
#define Y2JB_UPLOAD_H

#include <stddef.h>

#define UPLOAD_SLOT_MERGED "merged"
#define UPLOAD_SLOT_DOWNLOAD0 "download0"
#define UPLOAD_SLOT_FAKESIGNIN "fakesignin"

typedef enum {
  UPLOAD_IDLE,
  UPLOAD_RUNNING,
  UPLOAD_DONE,
  UPLOAD_ERROR,
} upload_state_t;

typedef struct {
  upload_state_t state;
  char slot[16];
  long long bytes;
  long long total;
  char message[192];
} upload_status_t;

/* Starts a slot upload. length is Content-Length and sha256 is the expected lowercase hex
 * digest. Validates the slot, size limit and free space (at least 16 MiB must remain) before
 * accepting bytes. */
int upload_begin(const char *slot, long long length, const char *sha256, char *why, size_t cap);

/* Writes or finishes the active upload. A failure removes the partial file. */
int upload_write(const char *data, size_t len, char *why, size_t cap);
int upload_finish(char *why, size_t cap);

/* Abandons the active upload. Safe when idle. */
void upload_abort(const char *why);

void upload_status(upload_status_t *out);
int upload_running(void);

/* Removes temporary and partial files from an interrupted run. */
void upload_cleanup(void);

const char *upload_merged_path(void);
const char *upload_fakesignin_path(void);

#endif
