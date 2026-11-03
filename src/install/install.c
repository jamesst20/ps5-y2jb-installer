/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "install.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "config.h"
#include "installed.h"
#include "loopback.h"
#include "sce_appinst.h"

#define POLL_SECONDS 1
#define TIMEOUT_SECONDS (30 * 60)
/* "none" can appear briefly before the system installer registers the job. */
#define NONE_GRACE_SECONDS 15

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static install_status_t g_status = {
    .state = INSTALL_IDLE,
    .progress = -1,
};
static char g_pkg[512];
static char g_title_id[TITLE_ID_CAP];
static char g_uri[96];
static time_t g_submitted;
static long long g_pkg_size;
static int g_had_pkg;
static long long g_pkg_size_before;
static char g_json_before[65];
static int g_delete;
static char g_expected_digest[65];

static void finish(install_state_t state, const char *text) {
  pthread_mutex_lock(&g_lock);
  g_status.state = state;
  snprintf(g_status.message, sizeof g_status.message, "%s", text);
  pthread_mutex_unlock(&g_lock);
}

static void set_message(const char *text) {
  pthread_mutex_lock(&g_lock);
  snprintf(g_status.message, sizeof g_status.message, "%s", text);
  pthread_mutex_unlock(&g_lock);
}

static void copy_bounded(char *dst, size_t cap, const char *src, size_t src_cap) {
  size_t len = 0;
  if (cap == 0) {
    return;
  }
  if (src) {
    while (len < src_cap && src[len] != '\0') {
      len++;
    }
    if (len >= cap) {
      len = cap - 1;
    }
    memcpy(dst, src, len);
  }
  dst[len] = '\0';
}

static void end_job(install_state_t state, const char *text);

static void mark_done(const char *console_status) {
  pthread_mutex_lock(&g_lock);
  g_status.progress = 100;
  snprintf(g_status.console_status, sizeof g_status.console_status, "%s",
           console_status ? console_status : "completed");
  g_status.error_code = 0;
  g_status.error_description[0] = '\0';
  pthread_mutex_unlock(&g_lock);
}

/* Copies the console's install status into g_status. `downloaded_size` is the transfer
 * progress; `promote_progress` covers the unpack/verify phase that follows it. */
static void update_from_console(const sce_appinst_status_t *st) {
  pthread_mutex_lock(&g_lock);
  copy_bounded(g_status.console_status, sizeof g_status.console_status, st->status,
               sizeof st->status);
  g_status.downloaded_bytes = st->downloaded_size;
  g_status.total_bytes = st->total_size;
  if (strcmp(st->status, "promoting") == 0) {
    int progress = (int)st->promote_progress;
    g_status.progress = progress < 0 ? 0 : (progress > 100 ? 100 : progress);
  } else if (st->total_size > 0) {
    unsigned long long progress = st->downloaded_size * 100ull / st->total_size;
    g_status.progress = progress > 100 ? 100 : (int)progress;
  } else if (strcmp(st->status, "playable") == 0 || strcmp(st->status, "completed") == 0) {
    g_status.progress = 100;
  } else if (strcmp(st->status, "error") != 0) {
    g_status.progress = -1;
  }

  if (strcmp(st->status, "transferring") == 0) {
    snprintf(g_status.message, sizeof g_status.message, "Downloading the package to the console.");
  } else if (strcmp(st->status, "promoting") == 0) {
    snprintf(g_status.message, sizeof g_status.message, "Installing and verifying the package.");
  } else if (strcmp(st->status, "playable") == 0 || strcmp(st->status, "completed") == 0) {
    snprintf(g_status.message, sizeof g_status.message, "Finishing the installation.");
  }

  if (strcmp(st->status, "error") == 0 || st->error_info.error_code != 0) {
    g_status.error_code = st->error_info.error_code;
    copy_bounded(g_status.error_description, sizeof g_status.error_description,
                 st->error_info.description, sizeof st->error_info.description);
  } else {
    g_status.error_code = 0;
    g_status.error_description[0] = '\0';
  }
  pthread_mutex_unlock(&g_lock);
}

static int status_is_error(const sce_appinst_status_t *st, time_t elapsed) {
  if (strcmp(st->status, "error") == 0) {
    return 1;
  }
  return strcmp(st->status, "none") == 0 && elapsed >= NONE_GRACE_SECONDS;
}

static void fail_from_console(const sce_appinst_status_t *st) {
  char description[512];
  copy_bounded(description, sizeof description, st->error_info.description,
               sizeof st->error_info.description);
  char text[320];
  if (description[0]) {
    snprintf(text, sizeof text, "Installation failed: %s (0x%08X)", description,
             (unsigned)st->error_info.error_code);
  } else if (st->error_info.error_code) {
    snprintf(text, sizeof text, "Installation failed (0x%08X)",
             (unsigned)st->error_info.error_code);
  } else if (strcmp(st->status, "none") == 0) {
    snprintf(text, sizeof text, "The console did not start the installation.");
  } else {
    snprintf(text, sizeof text, "Installation failed.");
  }
  end_job(INSTALL_ERROR, text);
}

static void end_job(install_state_t state, const char *text) {
  loopback_stop();
  if (g_delete) {
    remove(g_pkg);
  }
  finish(state, text);
}

static int installed_since_start(void) {
  installed_info_t now;
  installed_read(g_title_id, &now);
  if (!now.installed || now.pkg_size != g_pkg_size) {
    return 0;
  }
  return !g_had_pkg || g_pkg_size_before != now.pkg_size || strcmp(now.app_json_sha256, g_json_before) != 0;
}

static void *worker(void *arg) {
  (void)arg;
  sceAppInstUtilInitialize();

  if (loopback_serve(g_pkg) != 0) {
    end_job(INSTALL_ERROR, "Could not start the local file server");
    return NULL;
  }

  sce_meta_info_t meta;
  sce_pkg_info_t pkg_info;
  sce_playgo_info_t *playgo = calloc(1, sizeof *playgo);
  memset(&meta, 0, sizeof meta);
  memset(&pkg_info, 0, sizeof pkg_info);
  if (!playgo) {
    end_job(INSTALL_ERROR, "Not enough memory");
    return NULL;
  }
  snprintf(g_uri, sizeof g_uri, "http://127.0.0.1:%d/install.pkg", Y2JB_LOOPBACK_PORT);
  meta.uri = g_uri;
  meta.ex_uri = "";
  meta.playgo_scenario_id = "";
  meta.content_id = "";
  meta.content_name = "YouTube";
  meta.icon_url = "";

  installed_info_t before;
  installed_read(g_title_id, &before);
  g_had_pkg = before.installed;
  g_pkg_size_before = before.pkg_size;
  snprintf(g_json_before, sizeof g_json_before, "%s", before.app_json_sha256);
  int same_package = g_expected_digest[0] != '\0' && before.installed
      && strcmp(before.package_digest, g_expected_digest) == 0;
  struct stat pkg_st;
  g_pkg_size = stat(g_pkg, &pkg_st) == 0 ? (long long)pkg_st.st_size : -1;
  if (g_pkg_size < 0) {
    end_job(INSTALL_ERROR, "The merged PKG is missing");
    return NULL;
  }
  int rc = sceAppInstUtilInstallByPackage(&meta, &pkg_info, playgo);
  free(playgo);
  if (rc != 0) {
    char text[160];
    snprintf(text, sizeof text, "The console refused the package (0x%08X)", (unsigned)rc);
    pthread_mutex_lock(&g_lock);
    g_status.error_code = rc;
    snprintf(g_status.console_status, sizeof g_status.console_status, "%s", "error");
    pthread_mutex_unlock(&g_lock);
    end_job(INSTALL_ERROR, text);
    return NULL;
  }
  g_submitted = time(NULL);

  char content_id[256];
  copy_bounded(content_id, sizeof content_id, pkg_info.content_id, sizeof pkg_info.content_id);
  if (content_id[0] == '\0') {
    fprintf(stderr, "install: the console returned no content id; progress is unavailable\n");
  }

  set_message("Waiting for the console to install the app");
  int saw_active = 0;
  time_t deadline = g_submitted + TIMEOUT_SECONDS;
  for (;;) {
    time_t elapsed = time(NULL) - g_submitted;
    if (content_id[0] != '\0') {
      sce_appinst_status_t st;
      memset(&st, 0, sizeof st);
      if (sceAppInstUtilGetInstallStatus(content_id, &st) == 0) {
        update_from_console(&st);
        if (strcmp(st.status, "transferring") == 0 || strcmp(st.status, "promoting") == 0) {
          saw_active = 1;
        }
        if (status_is_error(&st, elapsed)) {
          fail_from_console(&st);
          return NULL;
        }
        if (strcmp(st.status, "playable") == 0 || strcmp(st.status, "completed") == 0) {
          int stream_done = loopback_served() >= (uint64_t)g_pkg_size;
          if (installed_since_start() || saw_active || stream_done ||
              (same_package && elapsed >= NONE_GRACE_SECONDS)) {
            mark_done(st.status);
            end_job(INSTALL_DONE, same_package ? "YouTube is up to date." : "YouTube is installed.");
            return NULL;
          }
          if (elapsed >= NONE_GRACE_SECONDS) {
            end_job(INSTALL_ERROR, "The console did not confirm the installation.");
            return NULL;
          }
        }
      }
    }

    if (installed_since_start()) {
      mark_done("completed");
      end_job(INSTALL_DONE, "YouTube is installed.");
      return NULL;
    }

    if (time(NULL) >= deadline) {
      end_job(INSTALL_ERROR, "Timed out waiting for the app files");
      return NULL;
    }
    sleep(POLL_SECONDS);
  }
}

int install_start(const char *title_id, const char *pkg_path, const char *expected_digest,
                  int delete_when_done) {
  if (title_index(title_id) < 0 || !pkg_path) {
    return -1;
  }
  pthread_mutex_lock(&g_lock);
  if (g_status.state == INSTALL_RUNNING) {
    pthread_mutex_unlock(&g_lock);
    return -1;
  }
  snprintf(g_title_id, sizeof g_title_id, "%s", title_id);
  snprintf(g_pkg, sizeof g_pkg, "%s", pkg_path);
  snprintf(g_expected_digest, sizeof g_expected_digest, "%s", expected_digest ? expected_digest : "");
  g_delete = delete_when_done;
  g_status.state = INSTALL_RUNNING;
  snprintf(g_status.title_id, sizeof g_status.title_id, "%s", title_id);
  snprintf(g_status.message, sizeof g_status.message, "%s", "Starting the installer");
  g_status.console_status[0] = '\0';
  g_status.downloaded_bytes = 0;
  g_status.total_bytes = 0;
  g_status.progress = -1;
  g_status.error_code = 0;
  g_status.error_description[0] = '\0';
  pthread_mutex_unlock(&g_lock);

  pthread_t thread;
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  int rc = pthread_create(&thread, &attr, worker, NULL);
  pthread_attr_destroy(&attr);
  if (rc != 0) {
    finish(INSTALL_ERROR, "Could not start the installer");
    return -1;
  }
  return 0;
}

void install_status(install_status_t *out) {
  pthread_mutex_lock(&g_lock);
  *out = g_status;
  pthread_mutex_unlock(&g_lock);
}
