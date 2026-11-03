/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * AppInstUtil ABI declarations. Layouts follow ps5upload's installer (GPL-3.0), including
 * the zeroed tail of MetaInfo that Sony reads past its six pointers. */
#ifndef Y2JB_SCE_APPINST_H
#define Y2JB_SCE_APPINST_H

#include <stdint.h>

#define PLAYGOSCENARIOID_SIZE 3
#define CONTENTID_SIZE 0x30
#define LANGUAGE_SIZE 8
#define SCE_NUM_LANGUAGES 30
#define SCE_NUM_IDS 64

typedef struct {
  const char *uri;
  const char *ex_uri;
  const char *playgo_scenario_id;
  const char *content_id;
  const char *content_name;
  const char *icon_url;
  uint64_t unknown_30;
  uint64_t reserved[7];
} sce_meta_info_t;

typedef struct {
  char content_id[CONTENTID_SIZE];
  int content_type;
  int content_platform;
} sce_pkg_info_t;

typedef struct {
  char languages[SCE_NUM_LANGUAGES][LANGUAGE_SIZE];
  char playgo_scenario_ids[SCE_NUM_IDS][PLAYGOSCENARIOID_SIZE];
  char content_ids[SCE_NUM_IDS][CONTENTID_SIZE];
  unsigned char unknown[6480];
} sce_playgo_info_t;

typedef struct {
  int32_t error_code;
  int32_t version;
  char description[512];
  char type[9];
} sce_appinst_error_t;

typedef struct {
  char status[16];
  char src_type[8];
  uint32_t remain_time;
  uint64_t downloaded_size;
  uint64_t initial_chunk_size;
  uint64_t total_size;
  uint32_t promote_progress;
  sce_appinst_error_t error_info;
  int32_t local_copy_percent;
  uint8_t is_copy_only;
  /* Newer firmware may append fields; keep enough room for them. */
  uint8_t reserved[2048];
} sce_appinst_status_t;

int sceAppInstUtilInitialize(void);
int sceAppInstUtilInstallByPackage(sce_meta_info_t *meta, sce_pkg_info_t *pkg_info, sce_playgo_info_t *playgo);
int sceAppInstUtilGetInstallStatus(const char *content_id, sce_appinst_status_t *status);

#endif
