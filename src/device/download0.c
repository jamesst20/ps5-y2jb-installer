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
#define MARKER_LEN (sizeof MARKER - 1)

#define MARKER_WINDOW 128
#define READ_SIZE (1024 * 1024)
#define OVERLAP MARKER_WINDOW

#define LEGACY_DIGEST WORK_DIR "/download0.sha256"

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static char g_version[64];
static download0_version_state_t g_state = DOWNLOAD0_VERSION_UNKNOWN;

static unsigned g_generation;

static int valid_version(const char *value) {
  if (!value) {
    return 0;
  }
  size_t length = strlen(value);
  if (length < 2 || length >= sizeof g_version) {
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

static void store_version(const char *value) {
  pthread_mutex_lock(&g_lock);
  if (valid_version(value)) {
    snprintf(g_version, sizeof g_version, "%s", value);
    g_state = DOWNLOAD0_VERSION_READY;
  } else {
    g_version[0] = '\0';
    g_state = DOWNLOAD0_VERSION_UNKNOWN;
  }
  g_generation++;
  pthread_mutex_unlock(&g_lock);
}

static int find_version(const unsigned char *buffer, size_t length, char *out, size_t cap) {
  const unsigned char *p = buffer;
  const unsigned char *end = buffer + length;

  while (p < end) {
    const unsigned char *hit = memchr(p, MARKER[0], (size_t)(end - p));
    if (!hit) {
      return 0;
    }
    if ((size_t)(end - hit) >= MARKER_LEN && memcmp(hit, MARKER, MARKER_LEN) == 0) {
      const unsigned char *q = hit + MARKER_LEN;
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
            if (valid_version(out)) {
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
  unsigned generation = (unsigned)(uintptr_t)arg;
  char version[64] = "";
  int result = scan_version(DOWNLOAD0_DEST, version, sizeof version);

  pthread_mutex_lock(&g_lock);
  if (g_generation == generation) {
    if (result == 0 && valid_version(version)) {
      snprintf(g_version, sizeof g_version, "%s", version);
      g_state = DOWNLOAD0_VERSION_READY;
    } else {
      g_version[0] = '\0';
      g_state = DOWNLOAD0_VERSION_UNKNOWN;
    }
    g_generation++;
  }
  pthread_mutex_unlock(&g_lock);

  if (result < 0) {
    fprintf(stderr, "download0: could not read the image to find its version\n");
  } else if (result > 0) {
    fprintf(stderr, "download0: no autoloader_version marker in the image\n");
  }
  return NULL;
}

void download0_start_refresh(void) {
  remove(LEGACY_DIGEST);
  remove(LEGACY_DIGEST ".partial");

  struct stat st;
  if (stat(DOWNLOAD0_DEST, &st) != 0 || st.st_size <= 0) {
    store_version(NULL);
    return;
  }

  pthread_mutex_lock(&g_lock);
  g_state = DOWNLOAD0_VERSION_COMPUTING;
  unsigned generation = ++g_generation;
  pthread_mutex_unlock(&g_lock);

  fprintf(stderr, "download0: scanning %lld bytes for its version\n", (long long)st.st_size);
  pthread_t thread;
  if (pthread_create(&thread, NULL, refresh_thread, (void *)(uintptr_t)generation) != 0) {
    fprintf(stderr, "download0: could not start the version scan thread\n");
    store_version(NULL);
    return;
  }
  pthread_detach(thread);
}

int download0_scan_version_at(const char *path) {
  char version[64] = "";
  int result = scan_version(path, version, sizeof version);
  if (result == 0 && valid_version(version)) {
    store_version(version);
  } else {
    store_version(NULL);
  }
  return result;
}

void download0_version(char *out, size_t cap) {
  pthread_mutex_lock(&g_lock);
  if (cap > 0) {
    snprintf(out, cap, "%s", g_version);
  }
  pthread_mutex_unlock(&g_lock);
}

const char *download0_version_state_name(void) {
  const char *name;
  pthread_mutex_lock(&g_lock);
  switch (g_state) {
    case DOWNLOAD0_VERSION_COMPUTING: name = "computing"; break;
    case DOWNLOAD0_VERSION_READY: name = "ready"; break;
    default: name = "unknown"; break;
  }
  pthread_mutex_unlock(&g_lock);
  return name;
}

int download0_remove(void) {
  if (remove(DOWNLOAD0_DEST) != 0 && errno != ENOENT) {
    return -1;
  }
  remove(DOWNLOAD0_PARTIAL);
  remove(LEGACY_DIGEST);
  remove(LEGACY_DIGEST ".partial");
  store_version(NULL);
  return 0;
}
