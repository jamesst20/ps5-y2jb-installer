/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef Y2JB_HTTP_SERVER_H
#define Y2JB_HTTP_SERVER_H

#include <stddef.h>

typedef struct {
  int status;
  const char *content_type;
  char *body; /* owned by the server once returned */
  size_t body_len;
} http_reply_t;

/* A buffered request route. body is "" for requests without one. */
typedef http_reply_t (*http_route_fn)(const char *method, const char *path, const char *body, size_t body_len);

/* Optional streaming receiver for large uploads. Every callback returns 0 on success and
 * -1 with a reason in why. abort is called if the connection closes mid-body. */
typedef int (*http_body_begin_fn)(const char *method, const char *url, const char *query, long long content_length,
                                  char *why, size_t cap);
typedef int (*http_body_write_fn)(const char *data, size_t len, char *why, size_t cap);
typedef int (*http_body_finish_fn)(char *why, size_t cap);
typedef void (*http_body_abort_fn)(const char *why);

typedef struct {
  const char *method;
  const char *path;
  http_body_begin_fn begin;
  http_body_write_fn write;
  http_body_finish_fn finish;
  http_body_abort_fn abort;
} http_body_sink_t;

/* Serves route on all interfaces. sink may be NULL. Replies allow any origin, and buffered
 * request bodies are limited to 1 MiB. Returns 0 on success. */
int http_server_start(unsigned short port, http_route_fn route, const http_body_sink_t *sink);
void http_server_stop(void);

#endif
