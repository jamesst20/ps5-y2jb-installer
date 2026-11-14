/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "power.h"

#include <pthread.h>
#include <stdint.h>
#include <unistd.h>

#include "notify.h"

int sceSystemServiceRequestReboot(void);

static void *reboot_thread(void *arg) {
  sleep((unsigned int)(uintptr_t)arg);
  sceSystemServiceRequestReboot();
  return NULL;
}

static void *exit_thread(void *arg) {
  sleep((unsigned int)(uintptr_t)arg);
  notify_send("Stopped");

  sleep(1);

  _exit(0);
  return NULL;
}

static int start_thread(void *(*worker)(void *), unsigned int seconds) {
  pthread_t thread;
  pthread_attr_t attr;
  if (pthread_attr_init(&attr) != 0) {
    return -1;
  }
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  int rc = pthread_create(&thread, &attr, worker, (void *)(uintptr_t)seconds);
  pthread_attr_destroy(&attr);
  return rc == 0 ? 0 : -1;
}

int power_reboot_after(unsigned int seconds) {
  return start_thread(reboot_thread, seconds);
}

int power_exit_after(unsigned int seconds) {
  return start_thread(exit_thread, seconds);
}
