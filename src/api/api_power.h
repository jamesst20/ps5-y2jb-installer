/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef Y2JB_API_POWER_H
#define Y2JB_API_POWER_H

#include "http/http_server.h"

/* POST /api/reboot. */
http_reply_t api_reboot(void);

/* POST /api/shutdown. Stops the payload process, not the console. */
http_reply_t api_shutdown(void);

#endif
