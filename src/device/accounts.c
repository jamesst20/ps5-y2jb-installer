/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "accounts.h"

#include <string.h>

int sceUserServiceInitialize(void *params);
int sceUserServiceGetForegroundUser(uint32_t *user_id);
int sceUserServiceGetLoginUserIdList(int *id_list);
int sceUserServiceGetUserName(int32_t user_id, char *name, size_t max_size);

int sceRegMgrGetInt(int key, int *value);

#define REG_SIGNIN_FLAG_BASE 125874185
#define REG_SLOT_STRIDE 65536
#define REG_SLOT_COUNT 16

#define OFFLINE_ACTIVATION_FLAGS 0x1002

#define LOGIN_ID_EMPTY (-1)

static int g_user_service_ready = 0;

int accounts_init(void) {
  if (g_user_service_ready) {
    return 0;
  }
  int rc = sceUserServiceInitialize(NULL);

  if (rc == 0 || rc == (int)0x80960003) {
    g_user_service_ready = 1;
    return 0;
  }
  return rc;
}

int accounts_foreground(int32_t *uid) {
  uint32_t id = 0;
  *uid = 0;
  if (sceUserServiceGetForegroundUser(&id) != 0) {
    return -1;
  }
  *uid = (int32_t)id;
  return 0;
}

static int slot_for_name(const char *name) {
  if (!name[0]) {
    return 0;
  }
  for (int slot = 1; slot <= REG_SLOT_COUNT; slot++) {
    char stored[ACCOUNT_NAME_MAX];
    if (OffAct_GetAccountName(slot, stored) == 0 && stored[0] && strcmp(stored, name) == 0) {
      return slot;
    }
  }
  return 0;
}

static void fill_registry(account_info_t *info) {
  info->account_id = 0;
  info->activated = 0;
  info->signed_in = 0;
  if (info->slot == 0) {
    return;
  }
  OffAct_GetAccountId(info->slot, &info->account_id);
  info->activated = info->account_id != 0;

  int flag = 0;
  int key = REG_SIGNIN_FLAG_BASE + (info->slot - 1) * REG_SLOT_STRIDE;
  if (sceRegMgrGetInt(key, &flag) == 0) {
    info->signed_in = flag != 0;
  }
}

static int read_user(int32_t uid, account_info_t *info) {
  memset(info, 0, sizeof *info);
  info->uid = uid;
  info->logged_in = 1;
  if (sceUserServiceGetUserName(uid, info->name, sizeof info->name) != 0) {
    return -1;
  }
  info->slot = slot_for_name(info->name);
  fill_registry(info);
  return 0;
}

int accounts_list(account_info_t *out, int max) {
  int ids[ACCOUNT_LOGIN_MAX];
  for (int i = 0; i < ACCOUNT_LOGIN_MAX; i++) {
    ids[i] = LOGIN_ID_EMPTY;
  }
  if (sceUserServiceGetLoginUserIdList(ids) != 0) {
    return -1;
  }
  int count = 0;
  for (int slot = 1; slot <= REG_SLOT_COUNT && count < max; slot++) {
    account_info_t *a = &out[count];
    memset(a, 0, sizeof *a);
    if (OffAct_GetAccountName(slot, a->name) != 0 || !a->name[0]) {
      continue;
    }
    a->slot = slot;

    for (int i = 0; i < ACCOUNT_LOGIN_MAX; i++) {
      char name[ACCOUNT_NAME_MAX];
      if (ids[i] > 0 && sceUserServiceGetUserName(ids[i], name, sizeof name) == 0 && strcmp(name, a->name) == 0) {
        a->uid = ids[i];
        a->logged_in = 1;
        break;
      }
    }
    fill_registry(a);
    count++;
  }
  return count;
}

int accounts_find(int32_t uid, account_info_t *out) {
  int ids[ACCOUNT_LOGIN_MAX];
  for (int i = 0; i < ACCOUNT_LOGIN_MAX; i++) {
    ids[i] = LOGIN_ID_EMPTY;
  }
  if (sceUserServiceGetLoginUserIdList(ids) != 0) {
    return -1;
  }
  for (int i = 0; i < ACCOUNT_LOGIN_MAX; i++) {
    if (ids[i] == uid) {
      return read_user(uid, out) == 0 ? 0 : -1;
    }
  }
  return -1;
}

int accounts_activate(int32_t uid, int *sce_error) {
  account_info_t info;
  *sce_error = 0;
  if (accounts_find(uid, &info) != 0) {
    return -1;
  }
  if (info.slot == 0) {
    return -2;
  }
  if (info.activated) {
    return 1;
  }

  uint64_t id = OffAct_GenAccountId(info.name);
  int rc = OffAct_SetAccountId(info.slot, id);
  if (rc != 0) {
    *sce_error = rc;
    return -3;
  }
  char type[ACCOUNT_TYPE_MAX] = "np";
  rc = OffAct_SetAccountType(info.slot, type);
  if (rc != 0) {
    *sce_error = rc;
    return -3;
  }
  rc = OffAct_SetAccountFlags(info.slot, OFFLINE_ACTIVATION_FLAGS);
  if (rc != 0) {
    *sce_error = rc;
    return -3;
  }
  return 0;
}
