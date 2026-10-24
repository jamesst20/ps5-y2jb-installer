/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef Y2JB_POWER_H
#define Y2JB_POWER_H

/* Reboots the console after the given delay so the HTTP reply can be sent first. Returns
 * 0 when the worker thread starts. */
int power_reboot_after(unsigned int seconds);

/* Stops the payload process after the given delay, without running atexit handlers. The
 * HTTP and loopback servers stop with the process. Returns 0 when the worker thread starts. */
int power_exit_after(unsigned int seconds);

#endif
