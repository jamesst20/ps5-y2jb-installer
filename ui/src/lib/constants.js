// SPDX-License-Identifier: GPL-3.0-or-later

export const API_PORT = 47291;
export const POLL_MS = 2000;
// Each failed status poll costs its 7.5 s timeout, so this is roughly 35 s of silence.
export const STATUS_FAILURES_BEFORE_PAUSE = 5;
export const STEP_ORDER = ['accounts', 'youtube', 'y2jb', 'updates'];
