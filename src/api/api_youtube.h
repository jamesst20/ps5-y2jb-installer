/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef Y2JB_API_YOUTUBE_H
#define Y2JB_API_YOUTUBE_H

#include <stddef.h>

#include "http/http_server.h"

/* POST /api/youtube/install, optional body {"package_digest": "..."}. */
http_reply_t api_youtube_install(const char *body, size_t body_len);

/* POST /api/youtube/block-updates. Backs up appinfo.db, edits it in place, then reboots. */
http_reply_t api_youtube_block_updates(void);

#endif
