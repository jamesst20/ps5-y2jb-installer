/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef Y2JB_API_UPLOAD_H
#define Y2JB_API_UPLOAD_H

#include "http/http_server.h"

/* Streaming body sink for PUT /api/upload?slot=...&sha256=... */
const http_body_sink_t *api_upload_sink(void);

/* POST /api/upload/cancel. */
http_reply_t api_upload_cancel(void);

#endif
