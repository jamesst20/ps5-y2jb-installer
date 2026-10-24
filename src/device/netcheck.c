/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "netcheck.h"

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <pthread.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#define PROBE_TIMEOUT_US 500000
#define PROBE_INTERVAL_SECONDS 15

typedef enum {
  NET_UNKNOWN,
  NET_CHECKING,
  NET_ONLINE,
  NET_OFFLINE,
} net_state_t;

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static net_state_t g_state = NET_UNKNOWN;

static int reachable(const struct sockaddr *address, socklen_t length) {
  int fd = socket(address->sa_family, SOCK_STREAM, 0);
  if (fd < 0) {
    return 0;
  }
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
    close(fd);
    return 0;
  }

  int rc = connect(fd, address, length);
  if (rc == 0) {
    close(fd);
    return 1;
  }
  if (errno != EINPROGRESS) {
    close(fd);
    return 0;
  }

  fd_set writable;
  FD_ZERO(&writable);
  FD_SET(fd, &writable);
  struct timeval timeout = {0, PROBE_TIMEOUT_US};
  rc = select(fd + 1, NULL, &writable, NULL, &timeout);
  int online = 0;
  if (rc > 0) {
    int error = 0;
    socklen_t length = sizeof error;
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &length) == 0 && error == 0) {
      online = 1;
    }
  }
  close(fd);
  return online;
}

static int probe_internet(void) {
  struct addrinfo hints;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  struct addrinfo *results = NULL;
  if (getaddrinfo("example.com", "443", &hints, &results) != 0) {
    return 0;
  }
  int online = 0;
  for (struct addrinfo *entry = results; entry != NULL && !online; entry = entry->ai_next) {
    online = reachable(entry->ai_addr, entry->ai_addrlen);
  }
  freeaddrinfo(results);
  return online;
}

static void set_state(net_state_t state) {
  pthread_mutex_lock(&g_lock);
  g_state = state;
  pthread_mutex_unlock(&g_lock);
}

static void *monitor_thread(void *arg) {
  (void)arg;
  for (;;) {
    set_state(probe_internet() ? NET_ONLINE : NET_OFFLINE);
    sleep(PROBE_INTERVAL_SECONDS);
  }
  return NULL;
}

void net_start_monitor(void) {
  pthread_mutex_lock(&g_lock);
  g_state = NET_CHECKING;
  pthread_mutex_unlock(&g_lock);

  pthread_t thread;
  pthread_attr_t attr;
  if (pthread_attr_init(&attr) != 0) {
    set_state(NET_UNKNOWN);
    return;
  }
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  int rc = pthread_create(&thread, &attr, monitor_thread, NULL);
  pthread_attr_destroy(&attr);
  if (rc != 0) {
    set_state(NET_UNKNOWN);
  }
}

int net_online(void) {
  int online = -1;
  pthread_mutex_lock(&g_lock);
  if (g_state == NET_ONLINE) {
    online = 1;
  } else if (g_state == NET_OFFLINE) {
    online = 0;
  }
  pthread_mutex_unlock(&g_lock);
  return online;
}

const char *net_state_name(void) {
  const char *name;
  pthread_mutex_lock(&g_lock);
  switch (g_state) {
    case NET_CHECKING: name = "checking"; break;
    case NET_ONLINE: name = "online"; break;
    case NET_OFFLINE: name = "offline"; break;
    default: name = "unknown"; break;
  }
  pthread_mutex_unlock(&g_lock);
  return name;
}
