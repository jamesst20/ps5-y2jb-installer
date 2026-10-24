/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "api_status.h"

#include <string.h>
#include <sys/stat.h>

#include "api_accounts.h"
#include "api_util.h"
#include "device/appdb.h"
#include "cJSON.h"
#include "device/download0.h"
#include "fw/firmware.h"
#include "install/install.h"
#include "install/installed.h"
#include "device/kernel_rw.h"
#include "device/netcheck.h"
#include "device/sysinfo.h"
#include "device/upload.h"
#include "version.h"

static long long file_size(const char *path) {
  struct stat st;
  return stat(path, &st) == 0 ? (long long)st.st_size : -1;
}

static cJSON *file_json(const char *path) {
  long long size = file_size(path);
  cJSON *o = cJSON_CreateObject();
  cJSON_AddBoolToObject(o, "present", size >= 0);
  cJSON_AddNumberToObject(o, "size", (double)size);
  return o;
}

static const char *updates_name(appdb_updates_t updates) {
  switch (updates) {
    case APPDB_BLOCKED: return "blocked";
    case APPDB_NOT_BLOCKED: return "not_blocked";
    default: return "unknown";
  }
}

static cJSON *youtube_json(void) {
  installed_info_t info;
  installed_read(&info);
  cJSON *o = cJSON_CreateObject();
  cJSON_AddBoolToObject(o, "installed", info.installed);
  cJSON_AddNumberToObject(o, "pkg_size", (double)info.pkg_size);
  cJSON_AddStringToObject(o, "package_digest", info.package_digest);
  cJSON_AddStringToObject(o, "version", info.version);
  cJSON_AddStringToObject(o, "updates", updates_name(appdb_youtube_updates()));
  return o;
}

static const char *upload_state_name(upload_state_t state) {
  switch (state) {
    case UPLOAD_RUNNING: return "running";
    case UPLOAD_DONE: return "done";
    case UPLOAD_ERROR: return "error";
    default: return "idle";
  }
}

static const char *install_state_name(install_state_t state) {
  switch (state) {
    case INSTALL_RUNNING: return "running";
    case INSTALL_DONE: return "done";
    case INSTALL_ERROR: return "error";
    default: return "idle";
  }
}

static cJSON *jobs_json(void) {
  upload_status_t up;
  upload_status(&up);
  install_status_t in;
  install_status(&in);

  cJSON *o = cJSON_CreateObject();
  cJSON *u = cJSON_AddObjectToObject(o, "upload");
  cJSON_AddStringToObject(u, "state", upload_state_name(up.state));
  cJSON_AddStringToObject(u, "slot", up.slot);
  cJSON_AddNumberToObject(u, "bytes", (double)up.bytes);
  cJSON_AddNumberToObject(u, "total", (double)up.total);
  cJSON_AddStringToObject(u, "message", up.message);

  cJSON *i = cJSON_AddObjectToObject(o, "install");
  cJSON_AddStringToObject(i, "state", install_state_name(in.state));
  cJSON_AddStringToObject(i, "message", in.message);
  return o;
}

http_reply_t api_status(void) {
  char kernel_version[512] = "";
  char firmware[64] = "unknown";
  sysinfo_kernel_version(kernel_version, sizeof kernel_version);
  fw_from_kernel_version(kernel_version, firmware, sizeof firmware);

  http_reply_t reply = {500, "application/json", NULL, 0};
  cJSON *root = cJSON_CreateObject();
  if (!root) {
    return reply;
  }
  cJSON_AddBoolToObject(root, "ok", 1);
  cJSON_AddStringToObject(root, "version", Y2JB_VERSION);
  cJSON_AddStringToObject(root, "firmware", firmware);
  cJSON_AddBoolToObject(root, "kernel_rw", kernel_rw_available());
  int internet = net_online();
  cJSON_AddStringToObject(root, "internet_state", net_state_name());
  if (internet < 0) {
    cJSON_AddNullToObject(root, "internet");
  } else {
    cJSON_AddBoolToObject(root, "internet", internet);
  }

  cJSON *accounts = accounts_summary();
  if (accounts) {
    cJSON_AddItemToObject(root, "accounts", accounts);
  }

  cJSON *files = cJSON_AddObjectToObject(root, "files");
  cJSON_AddItemToObject(files, "merged", file_json(upload_merged_path()));
  int placed_present = file_size(DOWNLOAD0_DEST) >= 0;
  cJSON *placed = file_json(DOWNLOAD0_DEST);
  cJSON_AddStringToObject(placed, "path", DOWNLOAD0_DEST);
  char version[64] = "";
  const char *version_state = "unknown";
  if (placed_present) {
    download0_version(version, sizeof version);
    version_state = download0_version_state_name();
  }
  cJSON_AddStringToObject(placed, "version", version);
  cJSON_AddStringToObject(placed, "version_state", version_state);
  cJSON_AddItemToObject(files, "download0", placed);
  cJSON_AddItemToObject(files, "fakesignin", file_json(upload_fakesignin_path()));

  cJSON_AddItemToObject(root, "youtube", youtube_json());
  cJSON_AddItemToObject(root, "jobs", jobs_json());

  char *json = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  if (!json) {
    return reply;
  }
  reply.status = 200;
  reply.body = json;
  reply.body_len = strlen(json);
  return reply;
}
