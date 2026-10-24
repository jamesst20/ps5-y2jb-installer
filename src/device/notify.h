/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef Y2JB_NOTIFY_H
#define Y2JB_NOTIFY_H

/* Shows a transient "Y2JB Installer" system toast with sub_message underneath. */
int notify_send(const char *sub_message);

#endif
