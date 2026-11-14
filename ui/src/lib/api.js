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

// A jailbroken console stops accepting TCP connections for 10–20 s at a time, every so
// often, for every process on it. Requests made inside such a window never reach the
// payload. Before an action the payload is first asked for its status until it answers,
// then the action is sent once, so a lost reply cannot trigger the action twice.
const STALL_RETRY_MS = 45000;
const RETRY_PAUSE_MS = 1000;
const TIMEOUT_MS = 7500;

const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

/** True for failures that mean the request never got through, not a payload reply. */
export const isTransportError = (error) =>
  error instanceof TypeError || error?.name === 'AbortError' || error?.name === 'TimeoutError';

/**
 * @param {string} base
 * @param {string} method
 * @param {string} path
 * @param {unknown} [body]
 * @returns {Promise<Record<string, any> & { status: number, ok: boolean }>}
 */
async function request(base, method, path, body) {
  if (!base) {
    throw new Error(t.common.noConsoleSelected);
  }
  const res = await fetch(`${base}api/${path}`, {
    method,
    cache: 'no-store',
    headers: body ? { 'Content-Type': 'application/json' } : {},
    body: body ? JSON.stringify(body) : undefined,
    signal: AbortSignal.timeout(TIMEOUT_MS),
  });
  const data = await res.json().catch(() => ({}));
  return { status: res.status, ok: res.ok, ...data };
}

async function post(path, body) {
  // Stick to the console this call started against, even if the user switches consoles
  // while a retry is sleeping.
  const base = baseUrl;
  const deadline = Date.now() + STALL_RETRY_MS;
  for (;;) {
    try {
      await request(base, 'GET', 'status');
      break;
    } catch (error) {
      if (!isTransportError(error) || Date.now() >= deadline) {
        throw error;
      }
      await sleep(RETRY_PAUSE_MS);
    }
  }
  return request(base, 'POST', path, body);
}

export const getStatus = () => request(baseUrl, 'GET', 'status');
export const activate = (uid) => post('activate', { uid });
export const fakeSignin = (uid) => post('fakesignin', { uid });
export const reboot = () => post('reboot');
export const shutdownPayload = () => post('shutdown');
export const youtubeInstall = (packageDigest, titleId) =>
  post('youtube/install', { package_digest: packageDigest, title_id: titleId });
export const blockYoutubeUpdates = (titleIds) =>
  post('youtube/block-updates', { title_ids: titleIds });
export const removeDownload0 = (titleId) => post('download0/remove', { title_id: titleId });
