/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "api_download0.h"

#include <string.h>

#include "api_util.h"
#include "device/download0.h"
#include "device/upload.h"

http_reply_t api_download0_remove(void) {
  upload_status_t st;
  upload_status(&st);
  if (st.state == UPLOAD_RUNNING && strcmp(st.slot, UPLOAD_SLOT_DOWNLOAD0) == 0) {
    return result_reply(409, 0, "The download0.dat upload is still running.");
  }
  if (download0_remove() != 0) {
    return result_reply(500, 0, "Could not remove download0.dat from the YouTube folder.");
  }
  return result_reply(200, 1, "Removed download0.dat from the YouTube folder. Y2JB is not set up until it is uploaded again.");
}
