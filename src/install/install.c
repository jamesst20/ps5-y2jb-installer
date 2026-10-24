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

#define POLL_SECONDS 3
#define TIMEOUT_SECONDS (30 * 60)

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static install_status_t g_status = {INSTALL_IDLE, ""};
static char g_pkg[512];
static char g_uri[96];
static time_t g_started;
static long long g_pkg_size;
static int g_had_pkg;
static long long g_pkg_size_before;
static char g_json_before[65];
static int g_delete;
static char g_expected_digest[65];
static int g_same_package;

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

static void end_job(install_state_t state, const char *text) {
  loopback_stop();
  if (g_delete) {
    remove(g_pkg);
  }
  finish(state, text);
}

static int installed_since_start(void) {
  installed_info_t now;
  installed_read(&now);
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
  installed_read(&before);
  g_had_pkg = before.installed;
  g_pkg_size_before = before.pkg_size;
  snprintf(g_json_before, sizeof g_json_before, "%s", before.app_json_sha256);
  g_same_package = g_expected_digest[0] != '\0' && before.installed
      && strcmp(before.package_digest, g_expected_digest) == 0;
  struct stat pkg_st;
  g_pkg_size = stat(g_pkg, &pkg_st) == 0 ? (long long)pkg_st.st_size : -1;
  if (g_pkg_size < 0) {
    end_job(INSTALL_ERROR, "The merged PKG is missing");
    return NULL;
  }
  g_started = time(NULL);
  int rc = sceAppInstUtilInstallByPackage(&meta, &pkg_info, playgo);
  free(playgo);
  if (rc != 0) {
    char text[160];
    snprintf(text, sizeof text, "The console refused the package (0x%08X)", (unsigned)rc);
    end_job(INSTALL_ERROR, text);
    return NULL;
  }

  if (g_same_package) {
    end_job(INSTALL_DONE, "YouTube is reinstalled.");
    return NULL;
  }

  set_message("Waiting for the console to copy the app");
  time_t deadline = g_started + TIMEOUT_SECONDS;
  while (!installed_since_start()) {
    if (time(NULL) >= deadline) {
      end_job(INSTALL_ERROR, "Timed out waiting for the app files");
      return NULL;
    }
    sleep(POLL_SECONDS);
  }

  end_job(INSTALL_DONE, "YouTube is installed.");
  return NULL;
}

int install_start(const char *pkg_path, const char *expected_digest, int delete_when_done) {
  pthread_mutex_lock(&g_lock);
  if (g_status.state == INSTALL_RUNNING) {
    pthread_mutex_unlock(&g_lock);
    return -1;
  }
  snprintf(g_pkg, sizeof g_pkg, "%s", pkg_path);
  snprintf(g_expected_digest, sizeof g_expected_digest, "%s", expected_digest ? expected_digest : "");
  g_delete = delete_when_done;
  g_status.state = INSTALL_RUNNING;
  snprintf(g_status.message, sizeof g_status.message, "%s", "Starting the installer");
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
