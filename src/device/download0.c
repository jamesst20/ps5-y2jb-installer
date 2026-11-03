/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "download0.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MARKER "autoloader_version"
#define VERSION_STRING_MARKER "version_string"

#define MARKER_WINDOW 128
#define READ_SIZE (1024 * 1024)
#define OVERLAP MARKER_WINDOW

#define LEGACY_DIGEST WORK_DIR "/download0.sha256"

typedef struct {
  char version[64];
  download0_version_state_t state;
  unsigned generation;
} download0_entry_t;

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static download0_entry_t g_entries[TITLE_COUNT];
static unsigned g_epoch;

static int valid_version(const char *value) {
  if (!value) {
    return 0;
  }
  size_t length = strlen(value);
  if (length < 2 || length >= sizeof g_entries[0].version) {
    return 0;
  }
  if (value[0] != 'v' && !(value[0] >= '0' && value[0] <= '9')) {
    return 0;
  }
  for (size_t i = 0; i < length; i++) {
    char c = value[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          c == '.' || c == '-' || c == '_')) {
      return 0;
    }
  }
  return 1;
}

/* Gezine/Y2JB is the upstream base. Images from it expose the release as
 * `const version_string = "Y2JB 1.7 by Gezine";`, while the maintained itsPLK fork uses
 * `autoloader_version`. Accept a printable, non-empty string from either marker. */
static int valid_version_string(const char *value) {
  if (!value) {
    return 0;
  }
  size_t length = strlen(value);
  if (length < 2 || length >= sizeof g_entries[0].version) {
    return 0;
  }
  int has_alnum = 0;
  for (size_t i = 0; i < length; i++) {
    unsigned char c = (unsigned char)value[i];
    if (c < 0x20 || c > 0x7e || c == '"') {
      return 0;
    }
    if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
      has_alnum = 1;
    }
  }
  return has_alnum;
}

/* Sets one title's state. The caller holds g_lock. */
static void set_entry_locked(int index, const char *value, download0_version_state_t state) {
  if (value && (valid_version(value) || valid_version_string(value))) {
    snprintf(g_entries[index].version, sizeof g_entries[index].version, "%s", value);
    g_entries[index].state = DOWNLOAD0_VERSION_READY;
  } else {
    g_entries[index].version[0] = '\0';
    g_entries[index].state = state;
  }
  g_entries[index].generation++;
}

static int find_marker_value(const unsigned char *buffer, size_t length, const char *marker,
                             char *out, size_t cap, int strict) {
  size_t marker_len = strlen(marker);
  const unsigned char *p = buffer;
  const unsigned char *end = buffer + length;

  while (p < end) {
    const unsigned char *hit = memchr(p, marker[0], (size_t)(end - p));
    if (!hit) {
      return 0;
    }
    if ((size_t)(end - hit) >= marker_len && memcmp(hit, marker, marker_len) == 0) {
      const unsigned char *q = hit + marker_len;
      const unsigned char *limit = q + MARKER_WINDOW < end ? q + MARKER_WINDOW : end;
      while (q < limit && (*q == ' ' || *q == '\t' || *q == '\r' || *q == '\n')) {
        q++;
      }
      if (q < limit && *q == '=') {
        q++;
        while (q < limit && (*q == ' ' || *q == '\t')) {
          q++;
        }
        if (q < limit && *q == '"') {
          q++;
          const unsigned char *value = q;
          while (q < limit && *q != '"' && (size_t)(q - value) + 1 < cap) {
            q++;
          }
          if (q < limit && *q == '"' && q > value) {
            size_t value_len = (size_t)(q - value);
            memcpy(out, value, value_len);
            out[value_len] = '\0';
            if (strict ? valid_version(out) : valid_version_string(out)) {
              return 1;
            }
          }
        }
      }
    }
    p = hit + 1;
  }
  return 0;
}

static int find_version(const unsigned char *buffer, size_t length, char *out, size_t cap) {
  return find_marker_value(buffer, length, MARKER, out, cap, 1) ||
         find_marker_value(buffer, length, VERSION_STRING_MARKER, out, cap, 0);
}

static int scan_version(const char *path, char *out, size_t cap) {
  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    return -1;
  }
  unsigned char *buffer = malloc(READ_SIZE + OVERLAP);
  if (!buffer) {
    close(fd);
    return -1;
  }

  size_t carry = 0;
  int result = 1;
  for (;;) {
    ssize_t got = read(fd, buffer + carry, READ_SIZE);
    if (got < 0) {
      if (errno == EINTR) {
        continue;
      }
      result = -1;
      break;
    }
    if (got == 0) {
      break;
    }
    size_t total = carry + (size_t)got;
    if (find_version(buffer, total, out, cap)) {
      result = 0;
      break;
    }
    carry = total > OVERLAP ? OVERLAP : total;
    memmove(buffer, buffer + total - carry, carry);
  }

  free(buffer);
  close(fd);
  return result;
}

static void *refresh_thread(void *arg) {
  unsigned epoch = (unsigned)(uintptr_t)arg;

  for (int i = 0; i < TITLE_COUNT; i++) {
    char path[256];
    if (title_download0_path(TITLES[i].title_id, path, sizeof path) != 0) {
      continue;
    }

    pthread_mutex_lock(&g_lock);
    int present = epoch == g_epoch &&
                  g_entries[i].state == DOWNLOAD0_VERSION_COMPUTING;
    unsigned generation = g_entries[i].generation;
    pthread_mutex_unlock(&g_lock);
    if (!present) {
      continue;
    }

    char version[64] = "";
    int result = scan_version(path, version, sizeof version);

    pthread_mutex_lock(&g_lock);
    if (epoch == g_epoch && g_entries[i].generation == generation) {
      set_entry_locked(i, result == 0 ? version : NULL, DOWNLOAD0_VERSION_UNKNOWN);
    }
    pthread_mutex_unlock(&g_lock);

    if (result < 0) {
      fprintf(stderr, "download0: %s: could not read the image to find its version\n",
              TITLES[i].title_id);
    } else if (result > 0) {
      fprintf(stderr, "download0: %s: no Y2JB version marker in the image\n",
              TITLES[i].title_id);
    }
  }
  return NULL;
}

void download0_start_refresh(void) {
  remove(LEGACY_DIGEST);
  remove(LEGACY_DIGEST ".partial");

  pthread_mutex_lock(&g_lock);
  unsigned epoch = ++g_epoch;
  for (int i = 0; i < TITLE_COUNT; i++) {
    char path[256];
    struct stat st;
    if (title_download0_path(TITLES[i].title_id, path, sizeof path) == 0 &&
        stat(path, &st) == 0 && st.st_size > 0) {
      g_entries[i].version[0] = '\0';
      g_entries[i].state = DOWNLOAD0_VERSION_COMPUTING;
      g_entries[i].generation++;
      fprintf(stderr, "download0: scanning %s (%lld bytes) for its version\n",
              TITLES[i].title_id, (long long)st.st_size);
    } else {
      set_entry_locked(i, NULL, DOWNLOAD0_VERSION_UNKNOWN);
    }
  }
  pthread_mutex_unlock(&g_lock);

  pthread_t thread;
  if (pthread_create(&thread, NULL, refresh_thread, (void *)(uintptr_t)epoch) != 0) {
    fprintf(stderr, "download0: could not start the version scan thread\n");
    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < TITLE_COUNT; i++) {
      if (g_entries[i].state == DOWNLOAD0_VERSION_COMPUTING) {
        set_entry_locked(i, NULL, DOWNLOAD0_VERSION_UNKNOWN);
      }
    }
    pthread_mutex_unlock(&g_lock);
    return;
  }
  pthread_detach(thread);
}

int download0_scan_version_at(const char *title_id, const char *path) {
  int index = title_index(title_id);
  if (index < 0) {
    return -1;
  }
  char version[64] = "";
  int result = scan_version(path, version, sizeof version);
  pthread_mutex_lock(&g_lock);
  set_entry_locked(index, result == 0 ? version : NULL, DOWNLOAD0_VERSION_UNKNOWN);
  pthread_mutex_unlock(&g_lock);
  return result;
}

void download0_version(const char *title_id, char *out, size_t cap) {
  int index = title_index(title_id);
  if (cap == 0) {
    return;
  }
  pthread_mutex_lock(&g_lock);
  snprintf(out, cap, "%s", index >= 0 ? g_entries[index].version : "");
  pthread_mutex_unlock(&g_lock);
}

const char *download0_version_state_name(const char *title_id) {
  const char *name = "unknown";
  int index = title_index(title_id);
  if (index < 0) {
    return name;
  }
  pthread_mutex_lock(&g_lock);
  switch (g_entries[index].state) {
    case DOWNLOAD0_VERSION_COMPUTING: name = "computing"; break;
    case DOWNLOAD0_VERSION_READY: name = "ready"; break;
    default: name = "unknown"; break;
  }
  pthread_mutex_unlock(&g_lock);
  return name;
}

int download0_present(const char *title_id) {
  char path[256];
  struct stat st;
  return title_download0_path(title_id, path, sizeof path) == 0 && stat(path, &st) == 0;
}

int download0_remove(const char *title_id) {
  int index = title_index(title_id);
  char path[256], partial[256];
  if (index < 0 || title_download0_path(title_id, path, sizeof path) != 0 ||
      title_download0_partial(title_id, partial, sizeof partial) != 0) {
    return -1;
  }
  if (remove(path) != 0 && errno != ENOENT) {
    return -1;
  }
  remove(partial);
  pthread_mutex_lock(&g_lock);
  set_entry_locked(index, NULL, DOWNLOAD0_VERSION_UNKNOWN);
  pthread_mutex_unlock(&g_lock);
  return 0;
}
