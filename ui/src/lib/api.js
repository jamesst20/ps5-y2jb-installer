// SPDX-License-Identifier: GPL-3.0-or-later

import { API_PORT } from './constants.js';
import { strings as t } from './strings.js';

// Calls to the payload's API. The installer selects the console; the payload serves no
// page of its own.

let baseUrl = '';

/** Point payload requests at the console at `ip`; an empty address disables them. */
export function setApiBase(ip) {
  baseUrl = ip ? `http://${ip}:${API_PORT}/` : '';
}

/**
 * @param {string} method
 * @param {string} path
 * @param {unknown} [body]
 * @param {number} [timeoutMs]
 * @returns {Promise<Record<string, any> & { status: number, ok: boolean }>}
 */
async function call(method, path, body, timeoutMs = 7500) {
  if (!baseUrl) {
    throw new Error(t.common.noConsoleSelected);
  }
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), timeoutMs);
  try {
    const res = await fetch(`${baseUrl}api/${path}`, {
      method,
      cache: 'no-store',
      headers: body ? { 'Content-Type': 'application/json' } : {},
      body: body ? JSON.stringify(body) : undefined,
      signal: controller.signal,
    });
    const data = await res.json().catch(() => ({}));
    return { status: res.status, ok: res.ok, ...data };
  } finally {
    clearTimeout(timer);
  }
}

export const getStatus = () => call('GET', 'status');
export const activate = (uid) => call('POST', 'activate', { uid });
export const fakeSignin = (uid) => call('POST', 'fakesignin', { uid });
export const reboot = () => call('POST', 'reboot');
export const shutdownPayload = () => call('POST', 'shutdown');

export const youtubeInstall = (packageDigest) =>
  call('POST', 'youtube/install', packageDigest ? { package_digest: packageDigest } : undefined);
export const blockYoutubeUpdates = () => call('POST', 'youtube/block-updates');
export const removeDownload0 = () => call('POST', 'download0/remove');
