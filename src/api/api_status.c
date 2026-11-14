/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "api_status.h"

#include <string.h>

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
#include "titles.h"
#include "version.h"

static const char *updates_name(appdb_updates_t updates) {
  switch (updates) {
    case APPDB_BLOCKED: return "blocked";
    case APPDB_NOT_BLOCKED: return "not_blocked";
    default: return "unknown";
  }
}

static cJSON *youtube_region_json(const title_entry_t *title) {
  installed_info_t info;
  installed_read(title->title_id, &info);
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "title_id", title->title_id);
  cJSON_AddBoolToObject(o, "installed", info.installed);
  cJSON_AddStringToObject(o, "package_digest", info.package_digest);
  cJSON_AddStringToObject(o, "version", info.version);
  cJSON_AddStringToObject(o, "updates", updates_name(appdb_youtube_updates(title->title_id)));
  return o;
}

static cJSON *youtube_json(void) {
  cJSON *array = cJSON_CreateArray();
  for (int i = 0; i < TITLE_COUNT; i++) {
    cJSON_AddItemToArray(array, youtube_region_json(&TITLES[i]));
  }
  return array;
}

static cJSON *download0_entry_json(const title_entry_t *title) {
  int present = download0_present(title->title_id);
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "title_id", title->title_id);
  cJSON_AddBoolToObject(o, "present", present);
  char version[64] = "";
  const char *version_state = "unknown";
  if (present) {
    download0_version(title->title_id, version, sizeof version);
    version_state = download0_version_state_name(title->title_id);
  }
  cJSON_AddStringToObject(o, "version", version);
  cJSON_AddStringToObject(o, "version_state", version_state);
  return o;
}

static cJSON *download0_json(void) {
  cJSON *array = cJSON_CreateArray();
  for (int i = 0; i < TITLE_COUNT; i++) {
    cJSON_AddItemToArray(array, download0_entry_json(&TITLES[i]));
  }
  return array;
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

  cJSON *i = cJSON_AddObjectToObject(o, "install");
  cJSON_AddStringToObject(i, "state", install_state_name(in.state));
  cJSON_AddStringToObject(i, "title_id", in.title_id);
  cJSON_AddStringToObject(i, "message", in.message);
  cJSON_AddStringToObject(i, "console_status", in.console_status);
  cJSON_AddNumberToObject(i, "progress", in.progress);
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
  cJSON_AddStringToObject(root, "version", Y2JB_VERSION);
  cJSON_AddStringToObject(root, "firmware", firmware);
  cJSON_AddBoolToObject(root, "kernel_rw", kernel_rw_available());
  int internet = net_online();
  if (internet < 0) {
    cJSON_AddNullToObject(root, "internet");
  } else {
    cJSON_AddBoolToObject(root, "internet", internet);
  }

  cJSON *accounts = accounts_summary();
  if (accounts) {
    cJSON_AddItemToObject(root, "accounts", accounts);
  }

  cJSON_AddItemToObject(root, "download0", download0_json());
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
