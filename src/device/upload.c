/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "upload.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

#include "download0.h"
#include "paths.h"
#include "crypto/sha256.h"

#define FREE_SPACE_MARGIN (16LL * 1024 * 1024)

#define MERGED_PATH TMP_DIR "/merged.pkg"
#define FAKESIGNIN_PATH TMP_DIR "/np-fake-signin-ps5.elf"

typedef struct {
  const char *name;
  const char *dest;
  long long max;
} slot_t;

static const slot_t SLOTS[] = {
    {UPLOAD_SLOT_MERGED, MERGED_PATH, 512LL * 1024 * 1024},
    {UPLOAD_SLOT_DOWNLOAD0, DOWNLOAD0_DEST, 1024LL * 1024 * 1024},
    {UPLOAD_SLOT_FAKESIGNIN, FAKESIGNIN_PATH, 4LL * 1024 * 1024},
};

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static upload_status_t g_status = {UPLOAD_IDLE, "", 0, 0, ""};
static int g_fd = -1;
static const slot_t *g_slot;
static char g_partial[512];
static char g_expected[65];
static SHA256_CTX g_sha;

static const slot_t *find_slot(const char *name) {
  for (size_t i = 0; i < sizeof SLOTS / sizeof SLOTS[0]; i++) {
    if (strcmp(SLOTS[i].name, name) == 0) {
      return &SLOTS[i];
    }
  }
  return NULL;
}

static int make_dirs(const char *path) {
  char buf[512];
  if (snprintf(buf, sizeof buf, "%s", path) >= (int)sizeof buf) {
    return -1;
  }
  for (char *p = buf + 1; *p; p++) {
    if (*p != '/') {
      continue;
    }
    *p = '\0';
    if (mkdir(buf, 0755) != 0 && errno != EEXIST) {
      return -1;
    }
    *p = '/';
  }
  if (mkdir(buf, 0755) != 0 && errno != EEXIST) {
    return -1;
  }
  return 0;
}

static const char *dir_of(const char *path, char *out, size_t cap) {
  snprintf(out, cap, "%s", path);
  char *slash = strrchr(out, '/');
  if (slash) {
    *slash = '\0';
  }
  return out;
}

static long long free_space(const char *path) {
  char dir[512];
  struct statvfs st;
  if (statvfs(dir_of(path, dir, sizeof dir), &st) != 0) {
    return -1;
  }
  return (long long)st.f_bavail * (long long)st.f_frsize;
}

static int is_lower_hex_64(const char *s) {
  if (strlen(s) != 64) {
    return 0;
  }
  for (int i = 0; i < 64; i++) {
    if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f'))) {
      return 0;
    }
  }
  return 1;
}

static void to_hex(const unsigned char digest[32], char out[65]) {
  static const char HEX[] = "0123456789abcdef";
  for (int i = 0; i < 32; i++) {
    out[i * 2] = HEX[digest[i] >> 4];
    out[i * 2 + 1] = HEX[digest[i] & 0x0F];
  }
  out[64] = '\0';
}

static void discard_locked(void) {
  if (g_fd >= 0) {
    close(g_fd);
    g_fd = -1;
  }
  if (g_partial[0] != '\0') {
    unlink(g_partial);
    g_partial[0] = '\0';
  }
}

static void fail_locked(const char *why) {
  discard_locked();
  g_status.state = UPLOAD_ERROR;
  snprintf(g_status.message, sizeof g_status.message, "%s", why);
}

int upload_begin(const char *slot, long long length, const char *sha256, char *why, size_t cap) {
  const slot_t *found = find_slot(slot);
  if (!found) {
    snprintf(why, cap, "unknown slot");
    return -1;
  }
  if (length < 0) {
    snprintf(why, cap, "Content-Length is required");
    return -1;
  }
  if (length == 0) {
    snprintf(why, cap, "empty body");
    return -1;
  }
  if (length > found->max) {
    snprintf(why, cap, "body is larger than this slot allows");
    return -1;
  }
  if (!is_lower_hex_64(sha256)) {
    snprintf(why, cap, "sha256 must be 64 lowercase hex digits");
    return -1;
  }

  pthread_mutex_lock(&g_lock);
  if (g_status.state == UPLOAD_RUNNING) {
    pthread_mutex_unlock(&g_lock);
    snprintf(why, cap, "an upload is running");
    return -1;
  }

  char dir[512];
  if (make_dirs(dir_of(found->dest, dir, sizeof dir)) != 0) {
    pthread_mutex_unlock(&g_lock);
    snprintf(why, cap, "cannot create the destination directory");
    return -1;
  }
  long long available = free_space(found->dest);
  if (available >= 0 && available < length + FREE_SPACE_MARGIN) {
    pthread_mutex_unlock(&g_lock);
    snprintf(why, cap, "not enough free space: need %lld bytes, have %lld", length + FREE_SPACE_MARGIN,
             available);
    return -1;
  }
  if (snprintf(g_partial, sizeof g_partial, "%s.partial", found->dest) >= (int)sizeof g_partial) {
    pthread_mutex_unlock(&g_lock);
    snprintf(why, cap, "destination path is too long");
    return -1;
  }
  int fd = open(g_partial, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0) {
    pthread_mutex_unlock(&g_lock);
    snprintf(why, cap, "cannot open the partial file (errno %d)", errno);
    return -1;
  }

  g_fd = fd;
  g_slot = found;
  sha256_init(&g_sha);
  snprintf(g_expected, sizeof g_expected, "%s", sha256);
  snprintf(g_status.slot, sizeof g_status.slot, "%s", found->name);
  g_status.state = UPLOAD_RUNNING;
  g_status.bytes = 0;
  g_status.total = length;
  snprintf(g_status.message, sizeof g_status.message, "receiving %s", found->name);
  pthread_mutex_unlock(&g_lock);
  return 0;
}

int upload_write(const char *data, size_t len, char *why, size_t cap) {
  pthread_mutex_lock(&g_lock);
  if (g_status.state != UPLOAD_RUNNING || g_fd < 0) {
    snprintf(why, cap, "%s", g_status.state == UPLOAD_ERROR ? g_status.message : "no upload is running");
    pthread_mutex_unlock(&g_lock);
    return -1;
  }
  size_t off = 0;
  while (off < len) {
    ssize_t n = write(g_fd, data + off, len - off);
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      int err = errno;
      fail_locked("could not write the upload");
      pthread_mutex_unlock(&g_lock);
      snprintf(why, cap, "could not write the upload (errno %d)", err);
      return -1;
    }
    off += (size_t)n;
  }
  sha256_update(&g_sha, (const unsigned char *)data, len);
  g_status.bytes += (long long)len;
  pthread_mutex_unlock(&g_lock);
  return 0;
}

int upload_finish(char *why, size_t cap) {
  pthread_mutex_lock(&g_lock);
  if (g_status.state != UPLOAD_RUNNING || g_fd < 0 || !g_slot) {
    pthread_mutex_unlock(&g_lock);
    snprintf(why, cap, "no upload is running");
    return -1;
  }
  if (g_status.bytes != g_status.total) {
    fail_locked("the upload ended early");
    pthread_mutex_unlock(&g_lock);
    snprintf(why, cap, "the upload ended early");
    return -1;
  }
  int sync_rc = fsync(g_fd);
  int close_rc = close(g_fd);
  g_fd = -1;
  if (sync_rc != 0 || close_rc != 0) {
    fail_locked("could not flush the upload");
    pthread_mutex_unlock(&g_lock);
    snprintf(why, cap, "could not flush the upload");
    return -1;
  }

  unsigned char digest[32];
  char hex[65];
  sha256_final(&g_sha, digest);
  to_hex(digest, hex);
  if (strcmp(hex, g_expected) != 0) {
    fail_locked("sha256 mismatch");
    pthread_mutex_unlock(&g_lock);
    snprintf(why, cap, "sha256 mismatch");
    return -1;
  }
  if (rename(g_partial, g_slot->dest) != 0) {
    int err = errno;
    fail_locked("could not move the upload into place");
    pthread_mutex_unlock(&g_lock);
    snprintf(why, cap, "could not move the upload into place (errno %d)", err);
    return -1;
  }
  g_partial[0] = '\0';

  if (strcmp(g_slot->name, UPLOAD_SLOT_DOWNLOAD0) == 0) {
    download0_start_refresh();
  }
  g_status.state = UPLOAD_DONE;
  snprintf(g_status.message, sizeof g_status.message, "%s is ready", g_slot->name);
  pthread_mutex_unlock(&g_lock);
  return 0;
}

void upload_abort(const char *why) {
  pthread_mutex_lock(&g_lock);

  if (g_status.state == UPLOAD_RUNNING || g_fd >= 0) {
    fail_locked(why ? why : "the upload stopped");
  }
  pthread_mutex_unlock(&g_lock);
}

void upload_status(upload_status_t *out) {
  pthread_mutex_lock(&g_lock);
  *out = g_status;
  pthread_mutex_unlock(&g_lock);
}

int upload_running(void) {
  pthread_mutex_lock(&g_lock);
  int running = (g_status.state == UPLOAD_RUNNING);
  pthread_mutex_unlock(&g_lock);
  return running;
}

void upload_cleanup(void) {
  pthread_mutex_lock(&g_lock);
  discard_locked();
  pthread_mutex_unlock(&g_lock);
  unlink(MERGED_PATH);
  unlink(MERGED_PATH ".partial");
  unlink(FAKESIGNIN_PATH);
  unlink(FAKESIGNIN_PATH ".partial");
  unlink(DOWNLOAD0_PARTIAL);
}

const char *upload_merged_path(void) {
  return MERGED_PATH;
}

const char *upload_fakesignin_path(void) {
  return FAKESIGNIN_PATH;
}
