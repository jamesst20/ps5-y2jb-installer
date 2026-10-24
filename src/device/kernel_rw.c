/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "kernel_rw.h"

#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>

#include <ps5/kernel.h>

int kernel_rw_available(void) {
  pid_t pid = getpid();
  uint64_t authid = kernel_get_ucred_authid(pid);
  if (authid == 0) {
    return 0;
  }
  return kernel_set_ucred_authid(pid, authid) == 0;
}
