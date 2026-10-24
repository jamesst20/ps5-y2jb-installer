// SPDX-License-Identifier: GPL-3.0-or-later

// Native commands. They only exist in the Tauri shell; in a browser these reject, and the
// page says so rather than pretending.
import { invoke } from '@tauri-apps/api/core';
import { listen } from '@tauri-apps/api/event';
import { strings as t } from './strings.js';

export const isDesktop = typeof window !== 'undefined' && '__TAURI_INTERNALS__' in window;

function call(command, args) {
  if (!isDesktop) {
    return Promise.reject(new Error(t.common.needsDesktopApp));
  }
  return invoke(command, args);
}

export const appVersion = () => call('app_version');
export const getSettings = () => call('get_settings');
export const rememberConsole = (ip) => call('set_console', { ip });
export const discover = () => call('discover');
export const probeConsole = (ip) => call('probe', { ip });
export const payloadInfo = () => call('payload_info');
export const sendPayload = (ip) => call('send_payload', { ip });
export const waitForApi = (ip, timeoutMs) => call('wait_for_api', { ip, timeoutMs });

export const manifestView = () => call('manifest_view');
export const expectedBuild = (firmware) => call('expected_build', { firmware });
export const fetchYoutube = (build, region) => call('fetch_youtube', { build, region });
export const fetchDownload0 = (tag) => call('fetch_download0', { tag });
export const download0Releases = () => call('download0_releases');
export const fetchFakesignin = () => call('fetch_fakesignin');
export const cacheStatus = () => call('cache_status');
export const clearCache = () => call('clear_cache');
export const clearDownload0Cache = () => call('clear_download0_cache');
export const uploadMerged = (ip, build, region) => call('upload_merged', { ip, build, region });
export const uploadDownload0 = (ip) => call('upload_download0', { ip });
export const uploadFakesignin = (ip) => call('upload_fakesignin', { ip });
export const cancelTransfer = () => call('cancel_transfer');

export function onPayloadProgress(handler) {
  if (!isDesktop) {
    return Promise.resolve(() => {});
  }
  return listen('payload-progress', (event) => handler(event.payload));
}

export function onTransferProgress(handler) {
  if (!isDesktop) {
    return Promise.resolve(() => {});
  }
  return listen('transfer-progress', (event) => handler(event.payload));
}
