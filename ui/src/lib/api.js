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
// payload, so they are safe to repeat. GETs are retried directly; before a POST the payload
// is first asked for its status until it answers, then the action is sent once, so a lost
// reply cannot trigger the action twice.
const STALL_RETRY_MS = 45000;
const RETRY_PAUSE_MS = 1000;

const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

/** True for failures that mean the request never got through, not a payload reply. */
const isTransportError = (error) =>
  error instanceof TypeError || (error && error.name === 'AbortError');

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
  const deadline = Date.now() + STALL_RETRY_MS;
  if (method !== 'GET') {
    // Wait for the console to accept connections again, then act exactly once.
    for (;;) {
      try {
        await once('GET', 'status', undefined, timeoutMs);
        break;
      } catch (error) {
        if (!isTransportError(error) || Date.now() >= deadline) {
          throw error;
        }
        await sleep(RETRY_PAUSE_MS);
      }
    }
    return once(method, path, body, timeoutMs);
  }
  for (;;) {
    try {
      return await once(method, path, body, timeoutMs);
    } catch (error) {
      if (!isTransportError(error) || Date.now() >= deadline) {
        throw error;
      }
      await sleep(RETRY_PAUSE_MS);
    }
  }
}

async function once(method, path, body, timeoutMs) {
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

export const getStatus = () => once('GET', 'status', undefined, 7500);
export const activate = (uid) => call('POST', 'activate', { uid });
export const fakeSignin = (uid) => call('POST', 'fakesignin', { uid });
export const reboot = () => call('POST', 'reboot');
export const shutdownPayload = () => call('POST', 'shutdown');

export const youtubeInstall = (packageDigest, titleId) =>
  call(
    'POST',
    'youtube/install',
    packageDigest || titleId
      ? { package_digest: packageDigest, title_id: titleId }
      : undefined,
  );
export const blockYoutubeUpdates = () => call('POST', 'youtube/block-updates');
export const removeDownload0 = (titleId) =>
  call('POST', 'download0/remove', { title_id: titleId });
