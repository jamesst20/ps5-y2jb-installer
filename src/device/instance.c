/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "instance.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#define KI_PID_OFFSET 72
#define KI_COMM_OFFSET 447
#define MAX_ROUNDS 10

void instance_claim_name(void) {
  syscall(SYS_thr_set_name, -1, INSTANCE_NAME);
}

static pid_t find_other(void) {
  int mib[4] = {1, 14, 8, 0};
  pid_t self = getpid();
  pid_t found = -1;
  size_t size = 0;

  if (sysctl(mib, 4, NULL, &size, NULL, 0) != 0) {
    return -2;
  }
  uint8_t *buf = malloc(size);
  if (!buf) {
    return -2;
  }
  if (sysctl(mib, 4, buf, &size, NULL, 0) != 0) {
    free(buf);
    return -2;
  }

  for (uint8_t *ptr = buf; ptr < buf + size;) {
    int record;
    pid_t pid;
    memcpy(&record, ptr, sizeof record);
    if (record <= 0) {
      break;
    }
    memcpy(&pid, ptr + KI_PID_OFFSET, sizeof pid);
    const char *comm = (const char *)(ptr + KI_COMM_OFFSET);
    if (pid != self && strcmp(comm, INSTANCE_NAME) == 0) {
      found = pid;
    }
    ptr += record;
  }
  free(buf);
  return found;
}

int instance_take_over(void) {
  int stopped = 0;
  for (int round = 0; round < MAX_ROUNDS; round++) {
    pid_t pid = find_other();
    if (pid == -2) {
      return -1;
    }
    if (pid < 0) {
      return stopped;
    }
    fprintf(stderr, "y2jb-installer: stopping the previous instance (pid %d)\n", (int)pid);
    kill(pid, SIGKILL);
    stopped++;
    sleep(1);
  }
  return stopped;
}
