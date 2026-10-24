/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef Y2JB_API_ACCOUNTS_H
#define Y2JB_API_ACCOUNTS_H

#include <stddef.h>

#include "cJSON.h"
#include "http/http_server.h"

/* Builds the accounts object for GET /api/status. Returns NULL on allocation failure. */
cJSON *accounts_summary(void);

/* POST /api/activate, body {"uid": n}. */
http_reply_t api_activate(const char *body, size_t body_len);

/* POST /api/fakesignin, body {"uid": n}. */
http_reply_t api_fakesignin(const char *body, size_t body_len);

#endif
