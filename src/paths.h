/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef Y2JB_PATHS_H
#define Y2JB_PATHS_H

/* Working directory for installer bookkeeping. Only appinfo.db backups are permanent;
 * temporary uploads live in TMP_DIR and are removed when their operation ends. */
#define WORK_DIR "/data/y2jb-installer"
#define TMP_DIR WORK_DIR "/tmp"

#endif
