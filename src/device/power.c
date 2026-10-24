/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "power.h"

#include <pthread.h>
#include <unistd.h>

#include "notify.h"

int sceSystemServiceRequestReboot(void);

static unsigned int g_reboot_delay = 0;
static unsigned int g_exit_delay = 0;

static void *reboot_thread(void *arg) {
  (void)arg;
  sleep(g_reboot_delay);
  sceSystemServiceRequestReboot();
  return NULL;
}

static void *exit_thread(void *arg) {
  (void)arg;
  sleep(g_exit_delay);
  notify_send("Stopped");

  sleep(1);

  _exit(0);
  return NULL;
}

static int start_thread(void *(*worker)(void *)) {
  pthread_t thread;
  pthread_attr_t attr;
  if (pthread_attr_init(&attr) != 0) {
    return -1;
  }
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  int rc = pthread_create(&thread, &attr, worker, NULL);
  pthread_attr_destroy(&attr);
  return rc == 0 ? 0 : -1;
}

int power_reboot_after(unsigned int seconds) {
  g_reboot_delay = seconds;
  return start_thread(reboot_thread);
}

int power_exit_after(unsigned int seconds) {
  g_exit_delay = seconds;
  return start_thread(exit_thread);
}
