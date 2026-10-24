/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "api_accounts.h"

#include <stdint.h>
#include <stdio.h>

#include "device/accounts.h"
#include "api_util.h"
#include "cJSON.h"
#include "account/fakesignin.h"

static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

#define ACCOUNT_ID_B64_LEN 16
static void account_id_b64(uint64_t id, char out[ACCOUNT_ID_B64_LEN]) {
  unsigned char raw[8];
  for (int i = 0; i < 8; i++) {
    raw[i] = (unsigned char)(id >> (8 * i));
  }
  int j = 0;
  for (int i = 0; i < 8; i += 3) {
    unsigned v = (unsigned)raw[i] << 16;
    if (i + 1 < 8) v |= (unsigned)raw[i + 1] << 8;
    if (i + 2 < 8) v |= raw[i + 2];
    out[j++] = B64[(v >> 18) & 63];
    out[j++] = B64[(v >> 12) & 63];
    out[j++] = (i + 1 < 8) ? B64[(v >> 6) & 63] : '=';
    out[j++] = (i + 2 < 8) ? B64[v & 63] : '=';
  }
  out[j] = '\0';
}

static cJSON *account_json(const account_info_t *a) {
  cJSON *o = cJSON_CreateObject();
  if (!o) {
    return NULL;
  }
  char hex[24], b64[ACCOUNT_ID_B64_LEN];
  snprintf(hex, sizeof hex, "0x%016llx", (unsigned long long)a->account_id);
  account_id_b64(a->account_id, b64);
  cJSON_AddNumberToObject(o, "uid", a->uid);
  cJSON_AddStringToObject(o, "name", a->name);
  cJSON_AddNumberToObject(o, "slot", a->slot);
  cJSON_AddStringToObject(o, "account_id_hex", hex);
  cJSON_AddStringToObject(o, "account_id_b64", b64);
  cJSON_AddBoolToObject(o, "activated", a->activated);
  cJSON_AddBoolToObject(o, "signed_in", a->signed_in);
  cJSON_AddBoolToObject(o, "logged_in", a->logged_in);
  return o;
}

cJSON *accounts_summary(void) {
  cJSON *root = cJSON_CreateObject();
  cJSON *list = cJSON_CreateArray();
  if (!root || !list) {
    cJSON_Delete(root);
    cJSON_Delete(list);
    return NULL;
  }
  cJSON_AddItemToObject(root, "list", list);

  if (accounts_init() != 0) {
    cJSON_AddBoolToObject(root, "ok", 0);
    cJSON_AddStringToObject(root, "error", "SceUserService could not start.");
    return root;
  }

  int32_t fg = 0;
  accounts_foreground(&fg);
  cJSON_AddNumberToObject(root, "foreground_uid", fg);

  account_info_t users[ACCOUNT_LOGIN_MAX];
  int n = accounts_list(users, ACCOUNT_LOGIN_MAX);
  for (int i = 0; i < n; i++) {
    cJSON *item = account_json(&users[i]);
    if (item) {
      cJSON_AddItemToArray(list, item);
    }
  }
  cJSON_AddBoolToObject(root, "ok", n >= 0);
  if (n < 0) {
    cJSON_AddStringToObject(root, "error", "could not read the account list");
  }
  return root;
}

static int body_uid(const char *body, size_t body_len, int32_t *uid) {
  cJSON *root = cJSON_ParseWithLength(body, body_len);
  if (!root) {
    return -1;
  }
  const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, "uid");
  double value = item ? item->valuedouble : 0;
  int ok = cJSON_IsNumber(item) && value >= INT32_MIN && value <= INT32_MAX;
  if (ok) {
    *uid = (int32_t)value;
  }
  cJSON_Delete(root);
  return ok ? 0 : -1;
}

http_reply_t api_activate(const char *body, size_t body_len) {
  int32_t uid = 0;
  if (body_uid(body, body_len, &uid) != 0) {
    return result_reply(400, 0, "Expected a JSON body with a UID.");
  }
  if (accounts_init() != 0) {
    return result_reply(500, 0, "SceUserService could not start.");
  }
  int sce_error = 0;
  int rc = accounts_activate(uid, &sce_error);
  switch (rc) {
    case 0:
      return result_reply(200, 1, "Activated. You can complete the remaining steps before rebooting.");
    case 1:
      return result_reply(200, 1, "Already activated.");
    case -1:
      return result_reply(404, 0, "That user is not logged in.");
    case -2:
      return result_reply(409, 0, "No registry slot has this user's name. Rename the account to match.");
    default: {
      char msg[96];
      snprintf(msg, sizeof msg, "Registry write failed (0x%08X).", (unsigned)sce_error);
      return result_reply(500, 0, msg);
    }
  }
}

http_reply_t api_fakesignin(const char *body, size_t body_len) {
  int32_t uid = 0;
  if (body_uid(body, body_len, &uid) != 0) {
    return result_reply(400, 0, "Expected a JSON body with a UID.");
  }
  if (accounts_init() != 0) {
    return result_reply(500, 0, "SceUserService could not start.");
  }
  account_info_t info;
  if (accounts_find(uid, &info) != 0) {
    return result_reply(404, 0, "That user is not logged in.");
  }
  if (!info.activated) {
    return result_reply(409, 0, "Activate this account first.");
  }
  char msg[256];
  int rc = fakesignin_run(msg, sizeof msg);
  return result_reply(rc == 0 ? 200 : 502, rc == 0, msg);
}
