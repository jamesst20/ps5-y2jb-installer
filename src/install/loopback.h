/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef Y2JB_LOOPBACK_H
#define Y2JB_LOOPBACK_H

/* Serves path at http://127.0.0.1:Y2JB_LOOPBACK_PORT/install.pkg with byte-range support.
 * Returns 0 on success. */
int loopback_serve(const char *path);
void loopback_stop(void);

#endif
