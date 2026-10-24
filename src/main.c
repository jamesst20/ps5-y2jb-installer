/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <unistd.h>

#include "api/api_router.h"
#include "api/api_upload.h"
#include "config.h"
#include "device/download0.h"
#include "http/http_server.h"
#include "device/instance.h"
#include "device/netcheck.h"
#include "device/notify.h"
#include "device/upload.h"
#include "version.h"

int main(void) {
  fprintf(stderr, "y2jb-installer %s starting\n", Y2JB_VERSION);
  instance_claim_name();
  if (instance_take_over() < 0) {
    fprintf(stderr, "y2jb-installer: cannot read the process list; continuing\n");
  }

  upload_cleanup();
  download0_start_refresh();
  net_start_monitor();

  if (http_server_start(Y2JB_API_PORT, api_route, api_upload_sink()) != 0) {
    fprintf(stderr, "y2jb-installer: cannot start the API on port %d\n", Y2JB_API_PORT);
    return 1;
  }
  fprintf(stderr, "y2jb-installer: API listening on all interfaces, port %d\n", Y2JB_API_PORT);

  int notice = notify_send("Ready");
  if (notice != 0) {
    fprintf(stderr, "y2jb-installer: notification failed (%d)\n", notice);
  }
  for (;;) {
    pause();
  }
}
