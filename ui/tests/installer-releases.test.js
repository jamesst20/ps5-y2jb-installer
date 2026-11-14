// SPDX-License-Identifier: GPL-3.0-or-later
import assert from 'node:assert/strict';
import { registerHooks } from 'node:module';
import { fileURLToPath } from 'node:url';
import { after, afterEach, test } from 'node:test';
import { compileModule } from 'svelte/compiler';
import { clearMocks, mockIPC } from '@tauri-apps/api/mocks';

// Run the actual Svelte state class and desktop command wrappers. Only native IPC
// is mocked, so requests can complete in a deliberate order without a console.
const installerUrl = new URL('../src/lib/installer.svelte.js', import.meta.url);
const hooks = registerHooks({
  load(url, context, nextLoad) {
    const result = nextLoad(url, context);
    if (url !== installerUrl.href) return result;
    return {
      ...result,
      source: compileModule(result.source.toString(), {
        filename: fileURLToPath(installerUrl),
        generate: 'client',
      }).js.code,
    };
  },
});
globalThis.window = {};
mockIPC(() => {});
const { Installer, OFFICIAL_DOWNLOAD0_REPO } = await import(installerUrl.href);
afterEach(clearMocks);
after(() => {
  hooks.deregister();
  delete globalThis.window;
});

function deferred() {
  const { promise, resolve, reject } = Promise.withResolvers();
  return { promise, resolve, reject };
}

const official = { tag: 'v1', digest: 'a'.repeat(64), prerelease: false };
const custom = { tag: 'v2', digest: 'b'.repeat(64), prerelease: false };
const customRepo = 'example/custom-loader';

function setup({ saveSettings = () => Promise.resolve() } = {}) {
  const requests = [];
  const fetches = [];
  mockIPC((command, args) => {
    switch (command) {
      case 'download0_releases': {
        const request = { repo: args.repo, ...deferred() };
        requests.push(request);
        return request.promise;
      }
      case 'set_download0_repo':
        return saveSettings();
      case 'fetch_download0':
        fetches.push(args);
        return { path: '/unused/download0.dat', size: 1, digest: custom.digest, label: custom.tag };
      default:
        throw new Error(`Unexpected native command: ${command}`);
    }
  });
  const installer = new Installer();
  installer.build = 'v01.000.030';
  installer.manifest = {
    builds: [{ build: installer.build, regions: [{ region: 'usa', title_id: 'PPSA01650' }] }],
  };
  return { installer, requests, fetches };
}

// Let setRepo's settings-save await and the IPC wrapper settle. No clock timing
// or external network response determines the ordering in these tests.
const settle = () => new Promise((resolve) => setImmediate(resolve));

test('late responses cannot replace the selected repository releases or fetch tag', async () => {
  const { installer, requests, fetches } = setup();
  const previous = installer.loadReleases();
  const current = installer.setRepo(customRepo);
  await settle();
  assert.deepEqual(requests.map((request) => request.repo), [OFFICIAL_DOWNLOAD0_REPO, customRepo]);
  requests[1].resolve([custom]);
  await current;
  requests[0].resolve([official]);
  await previous;

  assert.equal(installer.activeRepo, customRepo);
  assert.deepEqual(installer.releases.map((release) => release.tag), [custom.tag]);
  assert.equal(installer.selectedDownload0Tag, custom.tag);
  installer.cache = { download0: { digest: official.digest } };
  assert.equal(installer.cacheReady, false);
  await installer.getDownload0('usa');
  assert.deepEqual(fetches, [{ repo: customRepo, tag: custom.tag }]);
});

test('late errors cannot erase a successfully loaded current repository', async () => {
  const { installer, requests } = setup();
  const previous = installer.loadReleases();
  const current = installer.setRepo(customRepo);
  await settle();
  requests[1].resolve([custom]);
  await current;
  requests[0].reject(new Error('The previous repository timed out'));
  await previous;

  assert.deepEqual(installer.releases.map((release) => release.tag), [custom.tag]);
  assert.equal(installer.releasesError, false);
  assert.equal(installer.releasesLoaded, true);
});

test('a repository change clears the selected release and cache match before settings finish saving', async () => {
  const save = deferred();
  const { installer, requests } = setup({ saveSettings: () => save.promise });
  installer.releases = [official];
  installer.releasesLoaded = true;
  installer.download0Tag = official.tag;
  installer.cache = { download0: { digest: official.digest } };
  const current = installer.setRepo(customRepo);

  assert.equal(installer.activeRepo, customRepo);
  assert.equal(installer.selectedDownload0Tag, '');
  assert.equal(installer.cacheReady, false);
  assert.equal(installer.releasesLoading, true);
  assert.equal(installer.releasesLoaded, false);
  save.resolve();
  await settle();
  requests[0].resolve([custom]);
  await current;
  assert.equal(installer.selectedDownload0Tag, custom.tag);
});

test('an older request cannot mark the current request as finished', async () => {
  const { installer, requests } = setup();
  const previous = installer.loadReleases();
  const current = installer.setRepo(customRepo);
  await settle();
  requests[0].resolve([official]);
  await previous;
  assert.equal(installer.releasesLoading, true);
  assert.equal(installer.releasesLoaded, false);
  assert.deepEqual(installer.releases, []);
  requests[1].resolve([custom]);
  await current;
  assert.equal(installer.releasesLoading, false);
  assert.equal(installer.releasesLoaded, true);
});

test('returning to a repository still ignores its earlier request', async () => {
  const { installer, requests } = setup();
  const first = installer.loadReleases();
  const second = installer.setRepo(customRepo);
  await settle();
  const third = installer.setRepo(OFFICIAL_DOWNLOAD0_REPO);
  await settle();
  const latest = { ...official, tag: 'v3' };
  requests[2].resolve([latest]);
  await third;
  requests[1].resolve([custom]);
  await second;
  requests[0].resolve([official]);
  await first;
  assert.equal(installer.activeRepo, OFFICIAL_DOWNLOAD0_REPO);
  assert.equal(installer.selectedDownload0Tag, latest.tag);
});

test('a normal refresh preserves a selected release that still exists', async () => {
  const { installer, requests } = setup();
  installer.download0Tag = official.tag;
  const refresh = installer.loadReleases();
  requests[0].resolve([custom, official]);
  await refresh;
  assert.equal(installer.selectedDownload0Tag, official.tag);
  assert.equal(installer.releasesError, false);
  assert.equal(installer.releasesLoading, false);
  assert.equal(installer.releasesLoaded, true);
});

test('failure of the current request still reports an error', async () => {
  const { installer, requests } = setup();
  const refresh = installer.loadReleases();
  requests[0].reject(new Error('Current repository is unavailable'));
  await refresh;
  assert.deepEqual(installer.releases, []);
  assert.equal(installer.releasesError, true);
  assert.equal(installer.releasesLoading, false);
  assert.equal(installer.releasesLoaded, true);
});

test('one release selection is shared by every region', () => {
  const { installer } = setup();
  installer.releases = [official, custom];
  installer.setDownload0Tag(custom.tag);

  assert.equal(installer.selectedDownload0Tag, custom.tag);
  assert.equal(installer.cacheReady, false);

  installer.cache = { download0: { digest: custom.digest } };
  assert.equal(installer.cacheReady, true);
  assert.equal(installer.cacheReady, true);

  installer.setDownload0Tag(official.tag);
  assert.equal(installer.selectedDownload0Tag, official.tag);
  assert.equal(installer.cacheReady, false);
});

test('update blocking starts with every installed region checked', () => {
  const { installer } = setup();
  installer.status = {
    youtube: [{ title_id: 'PPSA01650', installed: true, updates: 'not_blocked' }],
  };

  assert.deepEqual(installer.installedRegions.map((row) => row.region), ['usa']);
  assert.equal(installer.updateChecked('usa'), true);
  assert.deepEqual(installer.selectedUpdateRows.map((row) => row.title_id), ['PPSA01650']);

  installer.setUpdateChecked('usa', false);
  assert.deepEqual(installer.selectedUpdateRows, []);
});

test('status rows read the flat download0 and youtube lists', () => {
  const { installer } = setup();
  installer.manifest.builds[0].regions[0].package_digest = 'c'.repeat(64);
  installer.status = {
    youtube: [{ title_id: 'PPSA01650', installed: true, package_digest: 'c'.repeat(64) }],
    download0: [{ title_id: 'PPSA01650', present: true, version: 'v1', version_state: 'ready' }],
  };

  assert.equal(installer.youtubeRows[0].alreadyInstalled, true);
  assert.equal(installer.download0Rows[0].valid, true);
  assert.equal(installer.download0Rows[0].youtubeInstalled, true);

  installer.status = { ...installer.status, download0: [{ ...installer.status.download0[0], version_state: 'computing' }] };
  assert.equal(installer.download0Rows[0].valid, false);
  assert.equal(installer.download0Rows[0].installedLabel, 'Computing…');
});

test('a probed elfldr console always offers Send payload', () => {
  const { installer } = setup();
  installer.desktop = true;
  installer.ip = '10.0.0.5';
  installer.probed = { ip: '10.0.0.5', loader: true, alive: true };

  assert.equal(installer.loaderVisible, true);
  assert.equal(installer.canSendPayload, true);
});

test('disconnect keeps the address and stops polling it', () => {
  const { installer } = setup();
  installer.ip = '10.0.0.5';
  installer.paused = false;
  installer.status = { accounts: { list: [] } };

  installer.disconnect();

  assert.equal(installer.ip, '10.0.0.5');
  assert.equal(installer.paused, true);
  assert.equal(installer.reachable, false);
  assert.equal(installer.loaderVisible, false);
  assert.equal(installer.statusError, false);
});
