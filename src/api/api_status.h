/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef Y2JB_API_STATUS_H
#define Y2JB_API_STATUS_H

#include "http/http_server.h"

/* GET /api/status. Reads local state only. */
http_reply_t api_status(void);

#endif
