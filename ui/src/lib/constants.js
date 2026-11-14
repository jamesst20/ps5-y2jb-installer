// SPDX-License-Identifier: GPL-3.0-or-later

export const API_PORT = 47291;
export const POLL_MS = 2000;
// A failed status poll can cost its 7.5 s timeout, so five of them are roughly 35–45 s of
// silence before polling pauses.
export const STATUS_FAILURES_BEFORE_PAUSE = 5;
export const STEP_ORDER = ['accounts', 'youtube', 'y2jb', 'updates'];
