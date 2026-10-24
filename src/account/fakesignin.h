/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef Y2JB_FAKESIGNIN_H
#define Y2JB_FAKESIGNIN_H

#include <stddef.h>

/* Sends the uploaded np-fake-signin ELF to the loader on 127.0.0.1:9021, half-closes the
 * socket, then deletes the uploaded copy. The caller must reboot the console to finish the
 * sign-in. Returns 0 when sent. */
int fakesignin_run(char *msg, size_t msg_cap);

#endif
