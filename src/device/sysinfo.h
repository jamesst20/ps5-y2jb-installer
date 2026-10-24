/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef Y2JB_SYSINFO_H
#define Y2JB_SYSINFO_H

#include <stddef.h>

/* Reads sysctl kern.version into out. Returns 0 on success. */
int sysinfo_kernel_version(char *out, size_t cap);

#endif
