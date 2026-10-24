/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "api_power.h"

#include "api_util.h"
#include "device/power.h"

http_reply_t api_reboot(void) {
  if (power_reboot_after(2) != 0) {
    return result_reply(500, 0, "Could not start the reboot.");
  }
  return result_reply(200, 1, "Rebooting in a few seconds.");
}

http_reply_t api_shutdown(void) {
  if (power_exit_after(1) != 0) {
    return result_reply(500, 0, "Could not stop the payload.");
  }
  return result_reply(200, 1, "The payload is stopping.");
}
