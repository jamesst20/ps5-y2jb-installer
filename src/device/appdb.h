/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef Y2JB_APPDB_H
#define Y2JB_APPDB_H

#include <stddef.h>

typedef enum {
  APPDB_BLOCKED,
  APPDB_NOT_BLOCKED,
  APPDB_UNKNOWN,
} appdb_updates_t;

/* Reads YouTube's update-block state without modifying appinfo.db. */
appdb_updates_t appdb_youtube_updates(void);

/* Backs up appinfo.db to a new timestamped file, then edits the live database in place so
 * its ownership, mode and inode are preserved. Returns 0, or -1 with the reason in msg. */
int appdb_block_youtube_updates(char *msg, size_t cap);

#endif
