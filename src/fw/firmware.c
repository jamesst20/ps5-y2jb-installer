/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "firmware.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static size_t match_version(const char *s, int *major, int *minor) {
  size_t n = 0;
  int maj = 0;

  while (n < 2 && isdigit((unsigned char)s[n])) {
    maj = maj * 10 + (s[n] - '0');
    n++;
  }
  if (n == 0 || s[n] != '.') {
    return 0;
  }
  if (!isdigit((unsigned char)s[n + 1]) || !isdigit((unsigned char)s[n + 2])) {
    return 0;
  }
  if (isdigit((unsigned char)s[n + 3])) {
    return 0;
  }

  *major = maj;
  *minor = (s[n + 1] - '0') * 10 + (s[n + 2] - '0');
  return n + 3;
}

static int find_tagged(const char *kv, const char *tag, int *major, int *minor) {
  const char *p = strstr(kv, tag);
  if (!p) {
    return 0;
  }
  return match_version(p + strlen(tag), major, minor) != 0;
}

static int find_after_slash(const char *kv, int *major, int *minor) {
  for (const char *p = strchr(kv, '/'); p; p = strchr(p + 1, '/')) {
    size_t n = match_version(p + 1, major, minor);
    if (n == 0) {
      continue;
    }
    char after = p[1 + n];
    if (after == '-' || after == ' ' || after == ':') {
      return 1;
    }
  }
  return 0;
}

static int find_bare(const char *kv, int *major, int *minor) {
  for (const char *p = kv; *p; p++) {
    if (p > kv && isdigit((unsigned char)p[-1])) {
      continue;
    }
    if (match_version(p, major, minor)) {
      return 1;
    }
  }
  return 0;
}

static void copy_raw(const char *kv, char *out, size_t cap) {
  size_t len = strlen(kv);
  while (len > 0 && (kv[len - 1] == '\n' || kv[len - 1] == '\r' || kv[len - 1] == ' ')) {
    len--;
  }
  if (len > cap - 1) {
    len = cap - 1;
  }
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)kv[i];
    out[i] = (c < 0x20 || c >= 0x7f) ? '?' : (char)c;
  }
  out[len] = '\0';
}

void fw_from_kernel_version(const char *kernel_version, char *out, size_t cap) {
  if (!out || cap == 0) {
    return;
  }
  out[0] = '\0';
  if (!kernel_version || !kernel_version[0]) {
    snprintf(out, cap, "unknown");
    return;
  }

  int major = 0, minor = 0;
  if (find_tagged(kernel_version, "releases/", &major, &minor) || find_after_slash(kernel_version, &major, &minor) ||
      find_bare(kernel_version, &major, &minor)) {
    snprintf(out, cap, "%d.%02d", major, minor);
    return;
  }
  copy_raw(kernel_version, out, cap);
}

int fw_code(const char *firmware) {
  int major = 0, minor = 0;
  if (!firmware) {
    return -1;
  }
  size_t n = match_version(firmware, &major, &minor);
  if (n == 0 || firmware[n] != '\0') {
    return -1;
  }
  return major * 100 + minor;
}
