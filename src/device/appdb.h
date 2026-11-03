/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef Y2JB_APPDB_H
#define Y2JB_APPDB_H

#include <stddef.h>

typedef enum {
  APPDB_BLOCKED,
  APPDB_NOT_BLOCKED,
  APPDB_UNKNOWN,
} appdb_updates_t;

/* Reads one YouTube title's update-block state without modifying appinfo.db. */
appdb_updates_t appdb_youtube_updates(const char *title_id);

/* Backs up appinfo.db to a new timestamped file, then edits every installed YouTube title in
 * place so its ownership, mode and inode are preserved. Returns 0, or -1 with the reason in
 * msg. */
int appdb_block_youtube_updates(char *msg, size_t cap);

#endif
