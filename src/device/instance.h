/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef Y2JB_INSTANCE_H
#define Y2JB_INSTANCE_H

/* The kernel keeps 19 characters. The .elf suffix makes Payload Manager list this as a
 * user process. */
#define INSTANCE_NAME "y2jb-installer.elf"

void instance_claim_name(void);

/* Stops every other process named INSTANCE_NAME, using the console's KERN_PROC_PROC
 * kinfo_proc layout. Returns the number stopped, or -1 when the process list cannot be read. */
int instance_take_over(void);

#endif
