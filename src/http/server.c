/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "http_server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <microhttpd.h>

#define MAX_BODY (1024 * 1024)
#define CONNECTION_TIMEOUT_SECONDS 300

typedef struct {
  char *buf;
  size_t len;
  size_t cap;
  int streaming;
  int finished;
  int failed;
  char message[192];
} request_ctx_t;

static struct MHD_Daemon *g_daemon = NULL;
static http_route_fn g_route = NULL;
static const http_body_sink_t *g_sink = NULL;

static int append_body(request_ctx_t *ctx, const char *data, size_t n) {
  if (ctx->len + n > MAX_BODY) {
    return -1;
  }
  if (ctx->len + n + 1 > ctx->cap) {
    size_t cap = ctx->cap ? ctx->cap : 1024;
    while (cap < ctx->len + n + 1) {
      cap *= 2;
    }
    char *grown = realloc(ctx->buf, cap);
    if (!grown) {
      return -1;
    }
    ctx->buf = grown;
    ctx->cap = cap;
  }
  memcpy(ctx->buf + ctx->len, data, n);
  ctx->len += n;
  ctx->buf[ctx->len] = '\0';
  return 0;
}

static enum MHD_Result send_reply(struct MHD_Connection *conn, http_reply_t reply) {
  void *body = reply.body ? (void *)reply.body : (void *)"";
  enum MHD_ResponseMemoryMode mode = reply.body ? MHD_RESPMEM_MUST_FREE : MHD_RESPMEM_PERSISTENT;
  struct MHD_Response *resp = MHD_create_response_from_buffer(reply.body_len, body, mode);
  if (!resp) {
    free(reply.body);
    return MHD_NO;
  }
  if (reply.content_type) {
    MHD_add_response_header(resp, MHD_HTTP_HEADER_CONTENT_TYPE, reply.content_type);
  }
  MHD_add_response_header(resp, MHD_HTTP_HEADER_CACHE_CONTROL, "no-store");

  MHD_add_response_header(resp, "Access-Control-Allow-Origin", "*");
  MHD_add_response_header(resp, "Access-Control-Allow-Methods", "GET, POST, PUT, OPTIONS");
  MHD_add_response_header(resp, "Access-Control-Allow-Headers", "Content-Type");
  enum MHD_Result rc = MHD_queue_response(conn, (unsigned int)reply.status, resp);
  MHD_destroy_response(resp);
  return rc;
}

static http_reply_t error_reply(int status, const char *message) {
  http_reply_t reply = {status, "text/plain; charset=utf-8", NULL, 0};
  size_t n = strlen(message);
  reply.body = malloc(n + 1);
  if (reply.body) {
    memcpy(reply.body, message, n + 1);
    reply.body_len = n;
  }
  return reply;
}

static http_reply_t stored_reply(void) {
  http_reply_t reply = {200, "application/json", NULL, 0};
  const char *json = "{\"ok\":true}";
  size_t n = strlen(json);
  reply.body = malloc(n + 1);
  if (reply.body) {
    memcpy(reply.body, json, n + 1);
    reply.body_len = n;
  }
  return reply;
}

static long long content_length(struct MHD_Connection *conn) {
  const char *value = MHD_lookup_connection_value(conn, MHD_HEADER_KIND, MHD_HTTP_HEADER_CONTENT_LENGTH);
  if (!value) {
    return -1;
  }
  char *end = NULL;
  long long n = strtoll(value, &end, 10);
  if (end == value || *end != '\0' || n < 0) {
    return -1;
  }
  return n;
}

typedef struct {
  char *buf;
  size_t len;
  size_t cap;
} query_buf_t;

static enum MHD_Result collect_query(void *cls, enum MHD_ValueKind kind, const char *key, const char *value) {
  (void)kind;
  query_buf_t *query = cls;
  if (!key || !value) {
    return MHD_YES;
  }
  int written = snprintf(query->buf + query->len, query->cap - query->len, "%s%s=%s",
                         query->len > 0 ? "&" : "", key, value);
  if (written < 0 || (size_t)written >= query->cap - query->len) {
    return MHD_NO;
  }
  query->len += (size_t)written;
  return MHD_YES;
}

static enum MHD_Result on_request(void *cls, struct MHD_Connection *conn, const char *url, const char *method,
                                  const char *version, const char *upload, size_t *upload_size, void **con_cls) {
  (void)cls;
  (void)version;

  request_ctx_t *ctx = *con_cls;
  if (!ctx) {
    ctx = calloc(1, sizeof *ctx);
    if (!ctx) {
      return MHD_NO;
    }
    *con_cls = ctx;

    if (strcmp(method, "OPTIONS") == 0) {
      http_reply_t reply = {204, NULL, NULL, 0};
      return send_reply(conn, reply);
    }
    if (g_sink && strcmp(method, g_sink->method) == 0 && strcmp(url, g_sink->path) == 0) {
      char query_text[256] = "";
      query_buf_t query = {query_text, 0, sizeof query_text};
      MHD_get_connection_values(conn, MHD_GET_ARGUMENT_KIND, &collect_query, &query);
      char why[192] = "";
      if (g_sink->begin(method, url, query_text, content_length(conn), why, sizeof why) != 0) {
        return send_reply(conn, error_reply(400, why));
      }
      ctx->streaming = 1;
    }
    return MHD_YES;
  }

  if (*upload_size > 0) {
    if (ctx->streaming) {
      if (!ctx->failed) {
        char why[192] = "";
        if (g_sink->write(upload, *upload_size, why, sizeof why) != 0) {
          ctx->failed = 1;
          snprintf(ctx->message, sizeof ctx->message, "%s", why);
          g_sink->abort(why);
        }
      }
      *upload_size = 0;
      return MHD_YES;
    }
    if (append_body(ctx, upload, *upload_size) != 0) {
      return send_reply(conn, error_reply(413, "request body too large"));
    }
    *upload_size = 0;
    return MHD_YES;
  }

  http_reply_t reply;
  if (ctx->streaming) {
    ctx->finished = 1;
    if (ctx->failed) {
      reply = error_reply(400, ctx->message);
    } else {
      char why[192] = "";
      reply = g_sink->finish(why, sizeof why) == 0 ? stored_reply() : error_reply(400, why);
    }
  } else if (strcmp(method, "GET") != 0 && strcmp(method, "POST") != 0) {
    reply = error_reply(405, "method not allowed");
  } else {
    reply = g_route(method, url, ctx->buf ? ctx->buf : "", ctx->len);
  }
  return send_reply(conn, reply);
}

static void on_completed(void *cls, struct MHD_Connection *conn, void **con_cls,
                         enum MHD_RequestTerminationCode code) {
  (void)cls;
  (void)conn;
  (void)code;
  request_ctx_t *ctx = *con_cls;
  if (ctx) {
    if (ctx->streaming && !ctx->finished && g_sink && g_sink->abort) {
      g_sink->abort("the connection closed before the upload finished");
    }
    free(ctx->buf);
    free(ctx);
    *con_cls = NULL;
  }
}

int http_server_start(unsigned short port, http_route_fn route, const http_body_sink_t *sink) {
  if (g_daemon) {
    return -1;
  }

  g_route = route;
  g_sink = sink;
  unsigned int timeout = CONNECTION_TIMEOUT_SECONDS;
  g_daemon = MHD_start_daemon(MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_ERROR_LOG, port, NULL, NULL, &on_request,
                              NULL, MHD_OPTION_NOTIFY_COMPLETED, &on_completed, NULL, MHD_OPTION_CONNECTION_TIMEOUT,
                              timeout, MHD_OPTION_END);
  if (!g_daemon) {
    g_route = NULL;
    g_sink = NULL;
    return -1;
  }
  return 0;
}

void http_server_stop(void) {
  if (g_daemon) {
    MHD_stop_daemon(g_daemon);
    g_daemon = NULL;
    g_route = NULL;
    g_sink = NULL;
  }
}
