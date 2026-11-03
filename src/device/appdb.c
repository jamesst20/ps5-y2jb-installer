/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "appdb.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "paths.h"
#include "titles.h"
#include "crypto/sha256.h"
#include "sqlite3.h"

#define LIVE_PATH "/system_data/priv/mms/appinfo.db"
#define STORE_DIR WORK_DIR "/store"
#define BACKUP_PREFIX STORE_DIR "/appinfo.db.bak-"

#define BLOCKED_CONTENT_VERSION "99.999.999"
#define BLOCKED_VERSION_FILE_URI "http://localhost"

static int copy_file(const char *src, const char *dst) {
  FILE *in = fopen(src, "rb");
  if (!in) {
    return -1;
  }
  FILE *out = fopen(dst, "wb");
  if (!out) {
    fclose(in);
    return -1;
  }
  static char buf[1 << 16];
  size_t n;
  int ok = 1;
  while ((n = fread(buf, 1, sizeof buf, in)) > 0) {
    if (fwrite(buf, 1, n, out) != n) {
      ok = 0;
      break;
    }
  }
  ok = ok && !ferror(in) && fflush(out) == 0 && fsync(fileno(out)) == 0;
  fclose(in);
  if (fclose(out) != 0) {
    ok = 0;
  }
  return ok ? 0 : -1;
}

static int same_contents(const char *a, const char *b) {
  char ha[65], hb[65];
  if (compute_sha256_file(a, ha) != 0 || compute_sha256_file(b, hb) != 0) {
    return -1;
  }
  return strcmp(ha, hb) == 0;
}

static int make_backup(char *path, size_t cap) {
  time_t now = time(NULL);
  struct tm tm;
  localtime_r(&now, &tm);
  char stamp[32];
  strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", &tm);
  snprintf(path, cap, "%s%s", BACKUP_PREFIX, stamp);

  struct stat st;
  if (stat(path, &st) == 0) {
    return -1;
  }
  mkdir(WORK_DIR, 0755);
  mkdir(STORE_DIR, 0755);
  if (copy_file(LIVE_PATH, path) != 0 || same_contents(LIVE_PATH, path) != 1) {
    remove(path);
    return -1;
  }
  return 0;
}

static int set_value(sqlite3 *db, const char *title_id, const char *key, const char *value) {
  sqlite3_stmt *st = NULL;
  if (sqlite3_prepare_v2(db, "UPDATE tbl_appinfo SET val = ? WHERE titleId = ? AND key = ?", -1, &st, NULL) !=
      SQLITE_OK) {
    return -1;
  }
  sqlite3_bind_text(st, 1, value, -1, SQLITE_STATIC);
  sqlite3_bind_text(st, 2, title_id, -1, SQLITE_STATIC);
  sqlite3_bind_text(st, 3, key, -1, SQLITE_STATIC);
  int ok = sqlite3_step(st) == SQLITE_DONE && sqlite3_changes(db) == 1;
  sqlite3_finalize(st);
  return ok ? 0 : -1;
}

static int read_value(sqlite3 *db, const char *title_id, const char *key, char *out, size_t cap) {
  sqlite3_stmt *st = NULL;
  if (sqlite3_prepare_v2(db, "SELECT val FROM tbl_appinfo WHERE titleId = ? AND key = ?", -1, &st, NULL) !=
      SQLITE_OK) {
    return -1;
  }
  sqlite3_bind_text(st, 1, title_id, -1, SQLITE_STATIC);
  sqlite3_bind_text(st, 2, key, -1, SQLITE_STATIC);
  int rows = 0;
  while (sqlite3_step(st) == SQLITE_ROW) {
    if (rows == 0) {
      const unsigned char *text = sqlite3_column_text(st, 0);
      snprintf(out, cap, "%s", text ? (const char *)text : "");
    }
    rows++;
  }
  sqlite3_finalize(st);
  return rows == 1 ? 0 : -1;
}

static int values_match_db(sqlite3 *db, const char *title_id) {
  char version[64], uri[256];
  if (read_value(db, title_id, "CONTENT_VERSION", version, sizeof version) != 0 ||
      read_value(db, title_id, "VERSION_FILE_URI", uri, sizeof uri) != 0) {
    return -1;
  }
  return strcmp(version, BLOCKED_CONTENT_VERSION) == 0 &&
         strcmp(uri, BLOCKED_VERSION_FILE_URI) == 0;
}

static int values_match(const char *path, const char *title_id) {
  sqlite3 *db = NULL;
  if (sqlite3_open_v2(path, &db, SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX, NULL) != SQLITE_OK) {
    sqlite3_close(db);
    return -1;
  }
  int match = values_match_db(db, title_id);
  sqlite3_close(db);
  return match;
}

static int installed_titles(void) {
  int count = 0;
  for (int i = 0; i < TITLE_COUNT; i++) {
    if (title_has_app(TITLES[i].title_id)) {
      count++;
    }
  }
  return count;
}

static int edit_live(char *msg, size_t cap) {
  sqlite3 *db = NULL;
  if (sqlite3_open_v2(LIVE_PATH, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX, NULL) != SQLITE_OK) {
    snprintf(msg, cap, "Could not open appinfo.db for writing: %s", sqlite3_errmsg(db));
    sqlite3_close(db);
    return -1;
  }
  sqlite3_exec(db, "PRAGMA journal_mode=MEMORY", NULL, NULL, NULL);

  int rc = -1;
  if (sqlite3_exec(db, "BEGIN IMMEDIATE", NULL, NULL, NULL) == SQLITE_OK) {
    int patched = 0;
    for (int i = 0; i < TITLE_COUNT; i++) {
      const char *title_id = TITLES[i].title_id;
      if (!title_has_app(title_id)) {
        continue;
      }
      if (set_value(db, title_id, "CONTENT_VERSION", BLOCKED_CONTENT_VERSION) != 0 ||
          set_value(db, title_id, "VERSION_FILE_URI", BLOCKED_VERSION_FILE_URI) != 0) {
        break;
      }
      patched++;
    }
    int verified = patched > 0;
    if (verified) {
      for (int i = 0; i < TITLE_COUNT; i++) {
        if (title_has_app(TITLES[i].title_id) &&
            values_match_db(db, TITLES[i].title_id) != 1) {
          verified = 0;
          break;
        }
      }
    }
    if (verified && sqlite3_exec(db, "COMMIT", NULL, NULL, NULL) == SQLITE_OK) {
      rc = 0;
    } else {
      sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
    }
  }
  if (rc != 0) {
    snprintf(msg, cap, "The YouTube rows did not change as expected; the file is unchanged.");
  }
  sqlite3_close(db);
  return rc;
}

appdb_updates_t appdb_youtube_updates(const char *title_id) {
  if (title_index(title_id) < 0) {
    return APPDB_UNKNOWN;
  }
  int match = values_match(LIVE_PATH, title_id);
  if (match == 1) {
    return APPDB_BLOCKED;
  }
  if (match == 0) {
    return APPDB_NOT_BLOCKED;
  }
  return APPDB_UNKNOWN;
}

int appdb_block_youtube_updates(char *msg, size_t cap) {
  int installed = installed_titles();
  if (installed == 0) {
    snprintf(msg, cap, "YouTube is not installed in any region, so there are no updates to block.");
    return -1;
  }
  char backup[256];
  if (make_backup(backup, sizeof backup) != 0) {
    snprintf(msg, cap, "Could not back up appinfo.db to the store; nothing was changed.");
    return -1;
  }
  if (edit_live(msg, cap) != 0) {
    return -1;
  }
  for (int i = 0; i < TITLE_COUNT; i++) {
    const char *title_id = TITLES[i].title_id;
    if (title_has_app(title_id) && values_match(LIVE_PATH, title_id) != 1) {
      snprintf(msg, cap, "The YouTube rows did not read back as blocked; the backup is in the store.");
      return -1;
    }
  }
  snprintf(msg, cap, "YouTube updates are blocked. Backup kept: %s", backup);
  return 0;
}
