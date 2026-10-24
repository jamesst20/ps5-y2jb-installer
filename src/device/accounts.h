/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef Y2JB_ACCOUNTS_H
#define Y2JB_ACCOUNTS_H

#include <stdint.h>

#include "offact.h"

#define ACCOUNT_LOGIN_MAX 16

typedef struct {
  int32_t uid;          /* logged-in user ID, or 0 */
  char name[ACCOUNT_NAME_MAX];
  int slot;             /* matching registry slot, or 0 */
  uint64_t account_id;  /* 0 when not activated */
  int activated;
  int signed_in;        /* fake or real PSN sign-in flag */
  int logged_in;
} account_info_t;

/* Initializes SceUserService. Safe to call repeatedly. Returns 0 on success. */
int accounts_init(void);

/* Writes the foreground user ID to uid. Returns 0 on success. */
int accounts_foreground(int32_t *uid);

/* Fills out with up to max accounts in slot order. Returns the count, or -1 when the
 * login list cannot be read. */
int accounts_list(account_info_t *out, int max);

/* Fills out for one logged-in user. Returns 0 on success, -1 when not logged in. */
int accounts_find(int32_t uid, account_info_t *out);

/* Activates the offline account of a logged-in user by writing its account ID, type "np",
 * and activation flags. Returns 0 on success, 1 when it is already activated, -1 when the
 * user is not logged in, -2 when no registry slot matches the name, and -3 when a registry
 * write fails. sce_error receives the failing SCE code. */
int accounts_activate(int32_t uid, int *sce_error);

#endif
