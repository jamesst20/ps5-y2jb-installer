/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef Y2JB_API_ROUTER_H
#define Y2JB_API_ROUTER_H

#include "http/http_server.h"

/* Dispatches one payload API request. */
http_reply_t api_route(const char *method, const char *path, const char *body, size_t body_len);

#endif
