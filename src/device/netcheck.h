/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef Y2JB_NETCHECK_H
#define Y2JB_NETCHECK_H

/* Starts the background internet reachability monitor. It resolves a public host through
 * the console's configured DNS and opens a short TCP connection; there is no general
 * internet client. */
void net_start_monitor(void);

/* Returns 1 when the console appears to have internet access, 0 when it does not, and -1
 * while the first probe is pending or the state is unknown. */
int net_online(void);

/* "unknown", "checking", "online" or "offline". */
const char *net_state_name(void);

#endif
