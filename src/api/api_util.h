/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef Y2JB_API_UTIL_H
#define Y2JB_API_UTIL_H

#include "http/http_server.h"

/* Takes ownership of json. A NULL json becomes a 500 response. */
http_reply_t json_reply(int status, char *json);

/* Copies text into a heap-owned body. */
http_reply_t text_reply(int status, const char *text);

/* {"ok": bool, "message": "..."}. */
http_reply_t result_reply(int status, int ok, const char *message);

#endif
