/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "loopback.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <microhttpd.h>

#include "config.h"

#define SERVED_PATH "/install.pkg"
#define CHUNK (1024 * 1024)

typedef struct {
  int fd;
  uint64_t offset;
  uint64_t remaining;
} body_t;

static struct MHD_Daemon *g_daemon = NULL;
static char g_file[512];

static enum MHD_Result accept_loopback(void *cls, const struct sockaddr *addr, socklen_t len) {
  (void)cls;
  if (addr->sa_family != AF_INET || len < (socklen_t)sizeof(struct sockaddr_in)) {
    return MHD_NO;
  }
  const struct sockaddr_in *in = (const struct sockaddr_in *)addr;
  return in->sin_addr.s_addr == htonl(INADDR_LOOPBACK) ? MHD_YES : MHD_NO;
}

static ssize_t read_body(void *cls, uint64_t pos, char *buf, size_t max) {
  body_t *body = cls;
  (void)pos;
  if (body->remaining == 0) {
    return MHD_CONTENT_READER_END_OF_STREAM;
  }
  size_t want = body->remaining < max ? (size_t)body->remaining : max;
  ssize_t got = pread(body->fd, buf, want, (off_t)body->offset);
  if (got <= 0) {
    return MHD_CONTENT_READER_END_WITH_ERROR;
  }
  body->offset += (uint64_t)got;
  body->remaining -= (uint64_t)got;
  return got;
}

static void free_body(void *cls) {
  body_t *body = cls;
  if (body) {
    close(body->fd);
    free(body);
  }
}

static int parse_range(const char *value, uint64_t total, uint64_t *start, uint64_t *end) {
  if (!value || strncmp(value, "bytes=", 6) != 0 || strchr(value, ',')) {
    return 0;
  }
  const char *p = value + 6;
  char *after = NULL;
  unsigned long long s = strtoull(p, &after, 10);
  if (after == p || *after != '-') {
    return 0;
  }
  unsigned long long e = total - 1;
  if (after[1] != '\0') {
    e = strtoull(after + 1, NULL, 10);
  }
  if (s > e || e >= total) {
    return 0;
  }
  *start = s;
  *end = e;
  return 1;
}

static enum MHD_Result handle(void *cls, struct MHD_Connection *conn, const char *url, const char *method,
                              const char *version, const char *upload, size_t *upload_size, void **con_cls) {
  (void)cls;
  (void)version;
  (void)upload;
  (void)upload_size;
  (void)con_cls;

  if (strcmp(method, "GET") != 0 && strcmp(method, "HEAD") != 0) {
    struct MHD_Response *resp = MHD_create_response_from_buffer(0, "", MHD_RESPMEM_PERSISTENT);
    enum MHD_Result rc = MHD_queue_response(conn, MHD_HTTP_METHOD_NOT_ALLOWED, resp);
    MHD_destroy_response(resp);
    return rc;
  }
  if (strcmp(url, SERVED_PATH) != 0) {
    struct MHD_Response *resp = MHD_create_response_from_buffer(0, "", MHD_RESPMEM_PERSISTENT);
    enum MHD_Result rc = MHD_queue_response(conn, MHD_HTTP_NOT_FOUND, resp);
    MHD_destroy_response(resp);
    return rc;
  }

  struct stat st;
  if (stat(g_file, &st) != 0) {
    struct MHD_Response *resp = MHD_create_response_from_buffer(0, "", MHD_RESPMEM_PERSISTENT);
    enum MHD_Result rc = MHD_queue_response(conn, MHD_HTTP_INTERNAL_SERVER_ERROR, resp);
    MHD_destroy_response(resp);
    return rc;
  }
  uint64_t total = (uint64_t)st.st_size;
  uint64_t start = 0, end = total - 1;
  const char *range = MHD_lookup_connection_value(conn, MHD_HEADER_KIND, MHD_HTTP_HEADER_RANGE);
  int partial = range && parse_range(range, total, &start, &end);
  if (range && !partial) {
    struct MHD_Response *resp = MHD_create_response_from_buffer(0, "", MHD_RESPMEM_PERSISTENT);
    char cr[64];
    snprintf(cr, sizeof cr, "bytes */%llu", (unsigned long long)total);
    MHD_add_response_header(resp, MHD_HTTP_HEADER_CONTENT_RANGE, cr);
    enum MHD_Result rc = MHD_queue_response(conn, 416, resp);
    MHD_destroy_response(resp);
    return rc;
  }

  int fd = open(g_file, O_RDONLY);
  if (fd < 0) {
    struct MHD_Response *resp = MHD_create_response_from_buffer(0, "", MHD_RESPMEM_PERSISTENT);
    enum MHD_Result rc = MHD_queue_response(conn, MHD_HTTP_INTERNAL_SERVER_ERROR, resp);
    MHD_destroy_response(resp);
    return rc;
  }

  uint64_t length = end - start + 1;
  body_t *body = calloc(1, sizeof *body);
  if (!body) {
    close(fd);
    return MHD_NO;
  }
  body->fd = fd;
  body->offset = start;
  body->remaining = length;

  struct MHD_Response *resp = MHD_create_response_from_callback(length, CHUNK, &read_body, body, &free_body);
  if (!resp) {
    free_body(body);
    return MHD_NO;
  }
  MHD_add_response_header(resp, MHD_HTTP_HEADER_ACCEPT_RANGES, "bytes");
  MHD_add_response_header(resp, MHD_HTTP_HEADER_CONTENT_TYPE, "application/octet-stream");
  if (partial) {
    char cr[128];
    snprintf(cr, sizeof cr, "bytes %llu-%llu/%llu", (unsigned long long)start, (unsigned long long)end,
             (unsigned long long)total);
    MHD_add_response_header(resp, MHD_HTTP_HEADER_CONTENT_RANGE, cr);
  }
  enum MHD_Result rc = MHD_queue_response(conn, partial ? MHD_HTTP_PARTIAL_CONTENT : MHD_HTTP_OK, resp);
  MHD_destroy_response(resp);
  return rc;
}

int loopback_serve(const char *path) {
  if (g_daemon) {
    return -1;
  }
  snprintf(g_file, sizeof g_file, "%s", path);
  g_daemon = MHD_start_daemon(MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_ERROR_LOG, Y2JB_LOOPBACK_PORT, &accept_loopback,
                              NULL, &handle, NULL, MHD_OPTION_END);
  return g_daemon ? 0 : -1;
}

void loopback_stop(void) {
  if (g_daemon) {
    MHD_stop_daemon(g_daemon);
    g_daemon = NULL;
  }
}
