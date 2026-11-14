/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef Y2JB_APPDB_H
#define Y2JB_APPDB_H

#include <stddef.h>

typedef enum {
  APPDB_BLOCKED,
  APPDB_NOT_BLOCKED,
  APPDB_UNKNOWN,
} appdb_updates_t;

/* Every key/value row appinfo.db holds for a title, as a JSON array of {key, val} objects,
 * or NULL when the database cannot be read. Read-only. */
struct cJSON *appdb_title_rows(const char *title_id);

/* The title's install directory as appinfo.db records it (`_org_path`), for example
 * /mnt/ext1/user/app/PPSA01651 on the M.2 SSD. Returns 0 and fills out, or -1 when the
 * database has no such row. Suitable as a title_app_root_fn. */
int appdb_app_dir(const char *title_id, char *out, size_t cap);

/* Reads one YouTube title's update-block state without modifying appinfo.db. */
appdb_updates_t appdb_youtube_updates(const char *title_id);

/* Backs up appinfo.db to a new timestamped file, then edits every installed YouTube title in
 * place so its ownership, mode and inode are preserved. Returns 0, or -1 with the reason in
 * msg. */
int appdb_block_youtube_updates(char *msg, size_t cap);

#endif
