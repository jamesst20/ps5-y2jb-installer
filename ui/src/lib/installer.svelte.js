// SPDX-License-Identifier: GPL-3.0-or-later

import {
  getStatus,
  setApiBase,
  activate,
  fakeSignin,
  reboot,
  youtubeInstall,
  blockYoutubeUpdates,
  removeDownload0,
  shutdownPayload,
} from './api.js';
import {
  isDesktop,
  getSettings,
  rememberConsole,
  discover,
  probeConsole,
  payloadInfo,
  sendPayload,
  waitForApi,
  onPayloadProgress,
  manifestView,
  expectedBuild,
  fetchYoutube,
  fetchDownload0,
  fetchFakesignin,
  cacheStatus,
  clearCache,
  clearDownload0Cache,
  uploadMerged,
  uploadDownload0,
  uploadFakesignin,
  cancelTransfer,
  onTransferProgress,
  download0Releases,
  setDownload0Repo,
  appVersion,
} from './desktop.js';
import { strings as t } from './strings.js';
import { POLL_MS, STEP_ORDER } from './constants.js';
import { releaseLabel as formatReleaseLabel } from './format.js';

export class Installer {
  appVersion = $state('');
  ip = $state('');
  targetIp = $state('');
  found = $state([]);
  probed = $state(null);
  scanning = $state(false);
  connecting = $state(false);
  connectedIp = $state('');
  status = $state(null);
  statusError = $state(false);
  payload = $state(null);
  sending = $state(false);
  sent = $state(0);
  total = $state(0);

  manifest = $state(null);
  build = $state('');
  region = $state('usa');
  cache = $state(null);
  cacheLoading = $state(true);
  releases = $state([]);
  releasesLoading = $state(false);
  releasesError = $state(false);
  selectedTag = $state('');
  transfer = $state(null);
  busy = $state('');
  rebootPending = $state(false);
  confirmBlock = $state(false);
  initialized = $state(false);
  paused = $state(false);
  refreshing = $state(false);
  failures = $state(0);
  notices = $state({});
  openStep = $state('');
  accountSlot = $state('');

  desktop = isDesktop;
  timer = null;
  unlistenPayload = null;
  unlistenTransfer = null;
  stopped = false;

  reachable = $derived(this.status !== null);
  loaderVisible = $derived(!this.reachable && this.probed !== null && this.probed.loader);
  payloadPercent = $derived(
    this.total > 0 ? Math.min(100, (this.sent / this.total) * 100) : 0,
  );
  accounts = $derived(this.status?.accounts?.list ?? []);
  kernelReady = $derived(this.status?.kernel_rw === true);
  selectedBuild = $derived(
    this.manifest?.builds?.find((entry) => entry.build === this.build) ?? null,
  );
  regions = $derived(this.selectedBuild?.regions ?? []);
  selectedRegion = $derived(
    this.regions.find((entry) => entry.region === this.region) ?? null,
  );
  cachedPkg = $derived(
    this.cache?.pkgs?.find(
      (entry) => entry.build === this.build && entry.region === this.region,
    ) ?? null,
  );
  latestTag = $derived(this.releases.find((release) => !release.prerelease)?.tag ?? '');
  selectedRelease = $derived(
    this.releases.find((release) => release.tag === this.selectedTag) ?? null,
  );
  releaseItems = $derived(
    this.releases.map((release) => ({
      value: release.tag,
      label: formatReleaseLabel(release, this.latestTag, t),
    })),
  );

  // The image carries its own release string, so it stays identifiable after YouTube writes
  // into the mounted filesystem. The version is not derived from a whole-file hash.
  installedVersion = $derived(this.status?.files?.download0?.version ?? '');
  installedVersionState = $derived(this.status?.files?.download0?.version_state ?? '');

  // The cached file is identified by its real digest, matched against the release list.
  // A digest that is not listed is shown as an unknown version instead of a guess.
  cacheTag = $derived(
    this.cache?.download0?.digest
      ? (this.releases.find((release) => release.digest === this.cache.download0.digest)?.tag ??
        '')
      : '',
  );

  cacheLabel = $derived.by(() => {
    if (this.cacheLoading) return t.common.computing;
    if (!this.cache?.download0) return t.y2jb.none;
    return this.cacheTag || t.y2jb.unknownVersion;
  });

  cacheMatchesSelection = $derived(!!this.cacheTag && this.cacheTag === this.selectedTag);
  updateAvailable = $derived(
    !!this.installedVersion && !!this.latestTag && this.installedVersion !== this.latestTag,
  );
  selectedIsLatest = $derived(
    !!this.selectedTag && this.selectedTag === this.latestTag,
  );

  installedLabel = $derived.by(() => {
    if (!this.status?.files?.download0?.present) return t.common.notInstalled;
    if (this.installedVersionState === 'computing') return t.common.computing;
    return this.installedVersion || t.y2jb.unknownVersion;
  });

  installed = $derived(this.status?.youtube ?? null);
  internetOnline = $derived(this.status?.internet === true);

  installedMatch = $derived.by(() => {
    if (!this.installed?.package_digest) return null;
    return (
      (this.manifest?.builds ?? [])
        .flatMap((entry) => entry.regions.map((r) => ({ build: entry.build, ...r })))
        .find((entry) => entry.package_digest === this.installed.package_digest) ?? null
    );
  });

  alreadyInstalled = $derived(
    this.installed?.installed === true &&
      this.installedMatch !== null &&
      this.installedMatch.build === this.build &&
      this.installedMatch.region === this.region,
  );

  installJob = $derived(this.status?.jobs?.install ?? null);
  jobRunning = $derived(
    this.status?.jobs?.upload?.state === 'running' ||
      this.status?.jobs?.install?.state === 'running',
  );
  versionMismatch = $derived(
    this.reachable && this.appVersion !== '' && this.status.version !== this.appVersion,
  );

  stepDone = $derived({
    accounts: this.accounts.some(
      (account) => account.logged_in && account.activated && account.signed_in,
    ),
    youtube: this.installed?.installed === true,
    y2jb: this.status?.files?.download0?.present === true,
    updates: this.status?.youtube?.updates === 'blocked',
  });
  completed = $derived(Object.values(this.stepDone).filter(Boolean).length);
  selectedAccount = $derived(
    this.accounts.find((account) => String(account.slot) === this.accountSlot) ?? null,
  );
  accountItems = $derived(
    this.accounts.map((account) => ({ value: String(account.slot), label: account.name })),
  );
  regionItems = $derived(
    this.regions.map((entry) => ({
      value: entry.region,
      label: t.youtube.regions[entry.region] ?? entry.region,
    })),
  );

  accountsSummary = $derived(
    this.accounts
      .filter((account) => account.logged_in && account.activated && account.signed_in)
      .map((account) => account.name)
      .join(', '),
  );
  youtubeSummary = $derived(
    this.installedMatch
      ? `${this.installedMatch.build} · ${t.youtube.regions[this.installedMatch.region] ?? this.installedMatch.region}`
      : this.installed?.installed
        ? t.youtube.installed
        : '',
  );
  y2jbSummary = $derived(
    this.status?.files?.download0?.present
      ? this.installedVersion ||
        (this.installedVersionState === 'computing' ? t.common.computing : t.y2jb.done)
      : '',
  );
  updatesSummary = $derived(
    this.status?.youtube?.updates === 'blocked' ? t.updates.blocked : '',
  );

  youtubeTransfer = $derived(
    this.transfer && ['base', 'sc', 'merged'].includes(this.transfer.label)
      ? this.transfer
      : null,
  );
  download0Transfer = $derived(
    this.transfer?.label === 'download0' ? this.transfer : null,
  );
  signinTransfer = $derived(
    this.transfer?.label === 'fakesignin' ? this.transfer : null,
  );

  start = async () => {
    this.stopped = false;
    this.timer = setInterval(() => this.refresh(), POLL_MS);

    if (!this.desktop) {
      this.cacheLoading = false;
      return;
    }

    try {
      const settings = await getSettings();
      if (settings?.custom_download0_repos) this.customRepos = settings.custom_download0_repos;
      if (settings?.active_download0_repo) this.activeRepo = settings.active_download0_repo;
      if (settings?.console_ip && !this.ip) {
        this.ip = settings.console_ip;
      }
    } catch {
      // Settings are optional; a missing or unreadable file must not block startup.
    }

    if (!this.stopped) {
      this.loadInitialData();
    }
  };

  stop = () => {
    this.stopped = true;
    if (this.timer !== null) {
      clearInterval(this.timer);
      this.timer = null;
    }
    if (this.unlistenPayload) {
      this.unlistenPayload();
      this.unlistenPayload = null;
    }
    if (this.unlistenTransfer) {
      this.unlistenTransfer();
      this.unlistenTransfer = null;
    }
  };

  loadInitialData = async () => {
    // The release list, the manifests and the cache hash are independent; start them all
    // together. cacheStatus hashes the cached image, so it is the slow one.
    this.loadReleases();
    this.cacheLoading = true;
    try {
      // The version and payload are instant. Do not hold the payload behind the cache scan;
      // hashing cached PKGs can take a while, and the user needs the Send payload card as
      // soon as the loader is found.
      const [version, nextPayload] = await Promise.all([appVersion(), payloadInfo()]);
      if (this.stopped) return;
      this.appVersion = version;
      this.payload = nextPayload;

      const [nextManifest, nextCache] = await Promise.all([manifestView(), cacheStatus()]);
      if (this.stopped) return;
      this.manifest = nextManifest;
      this.cache = nextCache;
    } catch (error) {
      if (!this.stopped) {
        this.notify('connect', String(error), false);
      }
    } finally {
      if (!this.stopped) {
        this.cacheLoading = false;
      }
    }

    if (this.stopped) return;

    this.unlistenPayload = await onPayloadProgress((progress) => {
      this.sent = progress.sent;
      this.total = progress.total;
    });
    if (this.stopped) {
      this.unlistenPayload();
      this.unlistenPayload = null;
      return;
    }

    this.unlistenTransfer = await onTransferProgress((progress) => {
      this.transfer = progress;
    });
    if (this.stopped) {
      this.unlistenTransfer();
      this.unlistenTransfer = null;
    }
  };

  loadReleases = async () => {
    this.releasesLoading = true;
    this.releasesError = false;
    try {
      this.releases = await download0Releases(this.activeRepo);
      if (
        !this.selectedTag ||
        !this.releases.some((release) => release.tag === this.selectedTag)
      ) {
        this.selectedTag =
          this.releases.find((release) => !release.prerelease)?.tag ??
          this.releases[0]?.tag ??
          '';
      }
    } catch {
      this.releases = [];
      this.releasesError = true;
    } finally {
      this.releasesLoading = false;
    }
  };

  notify = (step, text, ok = true) => {
    if (!text) {
      return;
    }
    this.notices = { ...this.notices, [step]: { text, ok } };
  };

  clearNotice = (step) => {
    if (!(step in this.notices)) {
      return;
    }
    const next = { ...this.notices };
    delete next[step];
    this.notices = next;
  };

  selectStep = (step) => {
    if (!this.reachable) {
      return;
    }
    this.openStep = this.openStep === step ? '' : step;
  };

  goToStep = (step) => {
    if (!this.reachable) {
      return;
    }
    this.openStep = step;
    requestAnimationFrame(() => {
      document.getElementById(`step-${step}`)?.scrollIntoView({ behavior: 'smooth', block: 'start' });
    });
  };

  focusNext = () => {
    this.openStep = STEP_ORDER.find((id) => !this.stepDone[id]) ?? '';
  };

  advance = (step) => {
    if (this.stepDone[step] && this.openStep === step) {
      this.focusNext();
    }
  };

  // Keep the account dropdown on a real account, preferring the one logged in now.
  ensureAccount = () => {
    if (this.accounts.length === 0) {
      this.accountSlot = '';
      return;
    }
    if (!this.accounts.some((account) => String(account.slot) === this.accountSlot)) {
      const preferred = this.accounts.find((account) => account.logged_in) ?? this.accounts[0];
      this.accountSlot = String(preferred.slot);
    }
  };

  stepState = (step) => {
    return this.stepDone[step] ? 'done' : this.openStep === step ? 'active' : 'todo';
  };

  discoveryLabel = (device) => {
    if (this.reachable && device.ip === this.connectedIp) {
      return t.connection.connected;
    }
    // "Connected" is only for the console the app is actually driving; a scan that found a
    // payload just means it is available.
    return device.api ? t.connection.payloadLoaded : t.connection.loaderFound;
  };

  releaseLabel = (release) => formatReleaseLabel(release, this.latestTag, t);

  initialize = async () => {
    if (this.initialized || !this.status) {
      return;
    }
    if (!this.manifest) {
      this.manifest = await manifestView();
    }
    const expected = await expectedBuild(this.status.firmware);
    this.build = expected ?? '';
    const match = this.installedMatch;
    const available =
      this.manifest?.builds?.find((entry) => entry.build === this.build)?.regions ?? [];
    if (match && available.some((entry) => entry.region === match.region)) {
      this.region = match.region;
    } else if (
      available.length > 0 &&
      !available.some((entry) => entry.region === this.region)
    ) {
      this.region = available[0].region;
    }
    this.initialized = true;
  };

  refresh = async () => {
    if (this.paused || this.refreshing) {
      return;
    }
    if (!this.targetIp) {
      this.status = null;
      this.statusError = false;
      return;
    }
    this.refreshing = true;
    setApiBase(this.targetIp);
    try {
      this.status = await getStatus();
      this.statusError = false;
      this.connectedIp = this.targetIp;
      this.failures = 0;
      this.ensureAccount();
      if (this.desktop) {
        await this.initialize();
      }
    } catch {
      this.status = null;
      this.statusError = true;
      this.connectedIp = '';
      this.openStep = '';
      this.failures += 1;
      if (this.desktop) {
        this.probed = await probeConsole(this.targetIp).catch(() => null);
      }
      // One failed poll can be a hiccup; two in a row means the console is gone (reboot,
      // payload stopped, cable pulled). Stop polling until the user connects again.
      if (this.failures >= 2) {
        this.paused = true;
        this.rebootPending = false;
      }
    } finally {
      this.refreshing = false;
    }
  };

  useConsole = async (address) => {
    const next = address.trim();
    if (!next) {
      return;
    }
    if (next !== this.connectedIp) {
      this.rebootPending = false;
    }
    this.ip = next;
    this.targetIp = next;
    setApiBase(this.targetIp);
    rememberConsole(next).catch(() => {});
    this.connecting = true;
    this.paused = false;
    this.failures = 0;
    this.initialized = false;
    this.probed = null;
    this.clearNotice('connect');
    try {
      await this.refresh();
      if (this.status) {
        this.focusNext();
      }
      if (!this.status && !this.probed?.loader) {
        this.notify('connect', t.connection.consoleUnreachable, false);
      }
    } finally {
      this.connecting = false;
    }
  };

  scan = async () => {
    this.scanning = true;
    this.clearNotice('connect');
    try {
      this.found = await discover();
      if (this.found.length === 0) {
        this.notify('connect', t.connection.consoleUnreachable, false);
      }
    } catch (error) {
      this.notify('connect', String(error), false);
    } finally {
      this.scanning = false;
    }
  };

  send = async () => {
    const address = this.ip.trim() || this.targetIp;
    if (!address) {
      return;
    }
    this.sending = true;
    this.sent = 0;
    this.total = this.payload?.size ?? 0;
    this.targetIp = address;
    setApiBase(this.targetIp);
    rememberConsole(address).catch(() => {});
    this.clearNotice('connect');
    try {
      const bytes = await sendPayload(this.targetIp);
      this.sent = bytes;
      if (!this.total) {
        this.total = bytes;
      }
      this.notify('connect', t.connection.waiting);
      const up = await waitForApi(this.targetIp, 20000);
      if (!up) {
        this.notify('connect', t.connection.consoleUnreachable, false);
        return;
      }
      this.probed = { ip: this.targetIp, loader: true, api: true };
      this.paused = false;
      this.failures = 0;
      this.initialized = false;
      await this.refresh();
      this.ip = this.targetIp;
      // waitForApi succeeded, so the payload is up. Clear "waiting" even if the immediate
      // status refresh raced with a poll; the next poll fills the state.
      this.clearNotice('connect');
      this.focusNext();
    } catch (error) {
      this.notify('connect', String(error), false);
    } finally {
      this.sending = false;
    }
  };

  runAction = async (step, action) => {
    this.busy = 'action';
    this.clearNotice(step);
    try {
      const result = await action();
      this.notify(
        step,
        result.message ?? (result.ok ? '' : t.common.errorStatus(result.status)),
        result.ok === true,
      );
      if (result.ok && step === 'accounts') {
        this.rebootPending = true;
      }
    } catch {
      this.notify(step, t.connection.consoleUnreachable, false);
    } finally {
      this.busy = '';
      await this.refresh();
      this.advance(step);
    }
  };

  activateSelected = () => {
    if (!this.selectedAccount) {
      return;
    }
    return this.runAction('accounts', () => activate(this.selectedAccount.uid));
  };

  rebootConsole = async () => {
    this.busy = 'reboot';
    this.clearNotice('connect');
    try {
      const result = await reboot();
      this.notify(
        'connect',
        result.message ?? (result.ok ? '' : t.common.errorStatus(result.status)),
        result.ok === true,
      );
      if (result.ok) {
        this.rebootPending = false;
      }
    } catch {
      this.notify('connect', t.connection.consoleUnreachable, false);
    } finally {
      this.busy = '';
    }
  };

  removeDownload0File = () => this.runAction('y2jb', removeDownload0);

  blockUpdates = () => this.runAction('updates', blockYoutubeUpdates);

  fetchAndJoin = async () => {
    if (!this.build) {
      return;
    }
    this.busy = 'fetch';
    this.transfer = null;
    this.clearNotice('youtube');
    try {
      const result = await fetchYoutube(this.build, this.region);
      const pkgs = (this.cache?.pkgs ?? []).filter(
        (entry) => entry.build !== this.build || entry.region !== this.region,
      );
      this.cache = {
        download0: this.cache?.download0 ?? null,
        fakesignin: this.cache?.fakesignin ?? null,
        pkgs: [
          ...pkgs,
          {
            build: this.build,
            region: this.region,
            path: result.directory,
            base: true,
            sc: true,
            merged: true,
          },
        ],
      };
    } catch (error) {
      this.notify('youtube', String(error), false);
    } finally {
      this.busy = '';
      this.transfer = null;
    }
  };

  uploadAndInstall = async () => {
    if (!this.targetIp || !this.build) {
      return;
    }
    this.busy = 'upload';
    this.transfer = null;
    this.clearNotice('youtube');
    try {
      await uploadMerged(this.targetIp, this.build, this.region);
      const result = await youtubeInstall(this.selectedRegion?.package_digest);
      if (!result.ok) {
        this.notify(
          'youtube',
          result.message ?? t.common.errorStatus(result.status),
          false,
        );
      }
    } catch (error) {
      this.notify('youtube', String(error), false);
    } finally {
      this.busy = '';
      this.transfer = null;
      await this.refresh();
      this.advance('youtube');
    }
  };

  getDownload0 = async () => {
    if (!this.selectedTag) {
      return;
    }
    this.busy = 'download0-fetch';
    this.transfer = null;
    this.clearNotice('y2jb');
    try {
      const result = await fetchDownload0(this.activeRepo, this.selectedTag);
      // The fetch already verified this digest, so publish it immediately instead of
      // hashing the same 336 MB again through cacheStatus.
      this.cache = {
        ...(this.cache ?? { pkgs: [], fakesignin: null }),
        download0: { path: result.path, size: result.size, digest: result.digest },
      };
      this.cacheLoading = false;
      this.notify('y2jb', t.y2jb.releaseReady(result.label));
    } catch (error) {
      this.notify('y2jb', String(error), false);
    } finally {
      this.busy = '';
      this.transfer = null;
    }
  };

  sendDownload0 = async () => {
    if (!this.cacheMatchesSelection) {
      return;
    }
    this.busy = 'download0-upload';
    this.transfer = null;
    this.clearNotice('y2jb');
    try {
      await uploadDownload0(this.targetIp);
      this.notify('y2jb', t.y2jb.done);
    } catch (error) {
      this.notify('y2jb', String(error), false);
    } finally {
      this.busy = '';
      this.transfer = null;
      await this.refresh();
      this.advance('y2jb');
    }
  };

  fakeSigninFlow = async (account) => {
    if (!this.targetIp || !account) {
      return;
    }
    this.busy = 'signin';
    this.transfer = null;
    this.clearNotice('accounts');
    try {
      // The helper is not bundled. Always fetch it, put it on the console, then run it;
      // the payload deletes it after handing it to the loader.
      const helper = await fetchFakesignin();
      this.cache = {
        ...(this.cache ?? { download0: null, pkgs: [] }),
        fakesignin: {
          path: helper.path,
          size: helper.size,
          digest: helper.digest,
        },
      };
      await uploadFakesignin(this.targetIp);
      await this.refresh();
      const result = await fakeSignin(account.uid);
      this.notify(
        'accounts',
        result.message ?? (result.ok ? '' : t.common.errorStatus(result.status)),
        result.ok === true,
      );
      if (result.ok) {
        this.rebootPending = true;
      }
    } catch (error) {
      this.notify('accounts', String(error), false);
    } finally {
      this.busy = '';
      this.transfer = null;
      await this.refresh();
    }
  };

  wipeCache = async () => {
    this.busy = 'cache';
    try {
      await clearCache();
      this.cache = await cacheStatus();
    } catch (error) {
      this.notify('y2jb', String(error), false);
    } finally {
      this.busy = '';
    }
  };

  wipeDownload0Cache = async () => {
    this.busy = 'cache';
    this.clearNotice('y2jb');
    try {
      await clearDownload0Cache();
      // Only the download0 cache changed; leave the PKG and fake-sign-in entries alone.
      this.cache = { ...this.cache, download0: null };
      this.notify('y2jb', t.y2jb.cacheDeleted);
    } catch (error) {
      this.notify('y2jb', String(error), false);
    } finally {
      this.busy = '';
    }
  };

  stopTransfer = async () => {
    try {
      await cancelTransfer();
    } catch {
      // Nothing to stop.
    }
  };

  addCustomRepo = async (repoSlug) => {
    const slug = repoSlug.trim().replace(/^https?:\/\/github\.com\//, "").replace(/\/$/, "");
    if (!slug || this.customRepos.includes(slug)) return;
    this.customRepos = [...this.customRepos, slug];
    this.activeRepo = slug;
    await setDownload0Repo(this.activeRepo, this.customRepos).catch(() => {});
    await this.loadReleases();
  };

  switchRepo = async (repoSlug) => {
    this.activeRepo = repoSlug;
    await setDownload0Repo(this.activeRepo, this.customRepos).catch(() => {});
    await this.loadReleases();
  };

  disconnect = () => {
    this.paused = true;
    this.targetIp = '';
    setApiBase('');
    this.status = null;
    this.statusError = false;
    this.probed = null;
    this.connectedIp = '';
    this.failures = 0;
    this.initialized = false;
    this.rebootPending = false;
    this.confirmBlock = false;
    this.transfer = null;
    this.accountSlot = '';
    this.notices = {};
    this.openStep = '';
  };

  stopPayload = async () => {
    if (!this.targetIp) {
      return;
    }
    this.busy = 'power';
    try {
      await shutdownPayload();
      this.disconnect();
      this.notify('connect', t.connection.payloadStopped);
    } catch (error) {
      this.notify('connect', String(error), false);
    } finally {
      this.busy = '';
    }
  };
}
