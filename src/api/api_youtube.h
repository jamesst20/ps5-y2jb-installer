/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef Y2JB_API_YOUTUBE_H
#define Y2JB_API_YOUTUBE_H

#include <stddef.h>

#include "http/http_server.h"

/* POST /api/youtube/install, optional body {"package_digest": "...", "title_id": "..."}. */
http_reply_t api_youtube_install(const char *body, size_t body_len);

/* POST /api/youtube/block-updates. Backs up appinfo.db, edits it in place, then reboots. */
http_reply_t api_youtube_block_updates(void);

/* POST /api/appinfo {title_id}: the title's appinfo.db rows and which install paths exist. */
http_reply_t api_appinfo(const char *body, size_t body_len);

#endif
