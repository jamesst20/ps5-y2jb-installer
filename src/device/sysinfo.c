/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "sysinfo.h"

#include <sys/sysctl.h>

int sysinfo_kernel_version(char *out, size_t cap) {
  if (!out || cap == 0) {
    return -1;
  }
  size_t len = cap;
  if (sysctlbyname("kern.version", out, &len, NULL, 0) != 0) {
    out[0] = '\0';
    return -1;
  }
  out[len < cap ? len : cap - 1] = '\0';
  return 0;
}
