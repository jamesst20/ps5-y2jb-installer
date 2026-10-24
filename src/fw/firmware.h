/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef Y2JB_FIRMWARE_H
#define Y2JB_FIRMWARE_H

#include <stddef.h>

/* Extracts the console firmware ("9.60", "13.00") from a kernel version string. Empty input
 * yields "unknown"; unparseable input is copied as a sanitized single line. */
void fw_from_kernel_version(const char *kernel_version, char *out, size_t cap);

/* Converts "M.mm" or "MM.mm" to major * 100 + minor. Returns -1 for anything else. */
int fw_code(const char *firmware);

#endif
