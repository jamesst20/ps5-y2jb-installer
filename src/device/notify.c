/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "notify.h"

#include <stdio.h>
#include <string.h>

typedef struct {
  char unused[45];
  char message[3075];
} notify_request_t;

int sceKernelSendNotificationRequest(int, notify_request_t *request, size_t size, int flags);

int notify_send(const char *sub_message) {
  notify_request_t request;
  memset(&request, 0, sizeof request);
  snprintf(request.message, sizeof request.message, "Y2JB Installer\n%s", sub_message);
  return sceKernelSendNotificationRequest(0, &request, sizeof request, 0);
}
