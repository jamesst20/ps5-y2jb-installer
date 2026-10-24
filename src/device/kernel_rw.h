/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef Y2JB_KERNEL_RW_H
#define Y2JB_KERNEL_RW_H

/* Returns 1 when this process can write its own kernel credentials, which requires kernel
 * read/write access. Read failures are reported as unavailable. */
int kernel_rw_available(void);

#endif
