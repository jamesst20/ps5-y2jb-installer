/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "fakesignin.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include "config.h"
#include "device/upload.h"

static void set_msg(char *msg, size_t cap, const char *text) {
  snprintf(msg, cap, "%s", text);
}

static int send_to_loader(const char *path) {
  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    return -1;
  }
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    close(fd);
    return -1;
  }
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof addr);
  addr.sin_family = AF_INET;
  addr.sin_port = htons(Y2JB_LOADER_PORT);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (connect(sock, (struct sockaddr *)&addr, sizeof addr) != 0) {
    close(sock);
    close(fd);
    return -1;
  }

  char buf[16384];
  ssize_t n;
  int rc = 0;
  while ((n = read(fd, buf, sizeof buf)) > 0) {
    char *p = buf;
    while (n > 0) {
      ssize_t put = send(sock, p, (size_t)n, 0);
      if (put <= 0) {
        rc = -1;
        break;
      }
      p += put;
      n -= put;
    }
    if (rc != 0) {
      break;
    }
  }
  if (n < 0) {
    rc = -1;
  }

  shutdown(sock, SHUT_WR);
  close(sock);
  close(fd);
  return rc;
}

int fakesignin_run(char *msg, size_t msg_cap) {
  const char *path = upload_fakesignin_path();
  struct stat st;
  if (stat(path, &st) != 0 || st.st_size <= 0) {
    set_msg(msg, msg_cap, "Upload np-fake-signin-ps5.elf first.");
    return -1;
  }

  if (send_to_loader(path) != 0) {
    char text[160];
    snprintf(text, sizeof text, "Could not send to the ELF loader on port %d (errno %d).", Y2JB_LOADER_PORT, errno);
    set_msg(msg, msg_cap, text);
    return -1;
  }

  remove(path);
  set_msg(msg, msg_cap, "Fake sign-in sent to the loader. You can complete the remaining steps before rebooting.");
  return 0;
}
