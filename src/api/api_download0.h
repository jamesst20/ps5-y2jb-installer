/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef Y2JB_API_DOWNLOAD0_H
#define Y2JB_API_DOWNLOAD0_H

#include "http/http_server.h"

/* POST /api/download0/remove, body {"title_id": "..."}. */
http_reply_t api_download0_remove(const char *body, size_t body_len);

#endif
