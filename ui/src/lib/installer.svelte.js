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

export const OFFICIAL_DOWNLOAD0_REPO = 'itsPLK/ps5-y2jb-autoloader';

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
  cache = $state(null);
  cacheLoading = $state(true);
  releases = $state([]);
  releasesLoading = $state(false);
  releasesError = $state(false);
  releasesLoaded = $state(false);
  download0Tags = $state({});
  customRepos = $state([]);
  activeRepo = $state(OFFICIAL_DOWNLOAD0_REPO);
  transfer = $state(null);
  busy = $state('');
  busyRegion = $state('');
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
  consoleUp = $derived(!this.reachable && this.probed?.alive === true);
  payloadPercent = $derived(
    this.total > 0 ? Math.min(100, (this.sent / this.total) * 100) : 0,
  );
  accounts = $derived(this.status?.accounts?.list ?? []);
  kernelReady = $derived(this.status?.kernel_rw === true);
  selectedBuild = $derived(
    this.manifest?.builds?.find((entry) => entry.build === this.build) ?? null,
  );
  regions = $derived(this.selectedBuild?.regions ?? []);
  latestTag = $derived(
    this.releases.find((release) => !release.prerelease)?.tag ?? this.releases[0]?.tag ?? '',
  );
  releaseItems = $derived(
    this.releases.map((release) => ({
      value: release.tag,
      label: formatReleaseLabel(release, this.latestTag, t),
    })),
  );

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

  youtubeRows = $derived.by(() =>
    this.regions.map((entry) => {
      const status =
        (this.status?.youtube?.regions ?? []).find(
          (row) => row.title_id === entry.title_id,
        ) ?? null;
      const cached =
        (this.cache?.pkgs ?? []).find(
          (pkg) => pkg.build === this.build && pkg.region === entry.region,
        ) ?? null;
      const match = status?.package_digest
        ? ((this.manifest?.builds ?? [])
            .flatMap((build) =>
              build.regions.map((region) => ({ build: build.build, ...region })),
            )
            .find((candidate) => candidate.package_digest === status.package_digest) ?? null)
        : null;
      const alreadyInstalled =
        status?.installed === true &&
        match !== null &&
        match.build === this.build &&
        match.region === entry.region;
      const job = this.status?.jobs?.install ?? null;
      const installJob =
        job && job.title_id === entry.title_id && job.state !== 'idle' ? job : null;
      return { ...entry, status, cached, alreadyInstalled, installJob };
    }),
  );

  download0Rows = $derived.by(() =>
    this.regions.map((entry) => {
      const status =
        (this.status?.files?.download0 ?? []).find(
          (row) => row.title_id === entry.title_id,
        ) ?? null;
      const selectedTag = this.tagFor(entry.region);
      const selectedRelease =
        this.releases.find((release) => release.tag === selectedTag) ?? null;
      const cacheMatches = !!this.cacheTag && this.cacheTag === selectedTag;
      const youtube = (this.status?.youtube?.regions ?? []).find(
        (row) => row.title_id === entry.title_id,
      );
      const version = status?.version ?? '';
      const versionState = status?.version_state ?? '';
      const valid = status?.y2jb ?? (status?.version_state === 'ready');
      // Upstream Gezine version strings such as "Y2JB 1.7 by Gezine" are not release tags,
      // so do not compare them with the repository's latest tag.
      const releaseComparable = /^[vV0-9]/.test(version);
      const installedLabel = !status?.present
        ? t.common.notInstalled
        : versionState === 'computing'
          ? t.common.computing
          : valid
            ? version || t.y2jb.unknownVersion
            : t.y2jb.notY2jb;
      return {
        ...entry,
        status,
        selectedTag,
        selectedRelease,
        cacheMatches,
        youtubeInstalled: youtube?.installed === true,
        valid,
        installedLabel,
        updateAvailable:
          valid && releaseComparable && !!version && !!this.latestTag && version !== this.latestTag,
        upToDate:
          valid && releaseComparable && !!version && !!this.latestTag && version === this.latestTag,
        selectedIsLatest: !!selectedTag && selectedTag === this.latestTag,
      };
    }),
  );

  internetOnline = $derived(this.status?.internet === true);

  jobRunning = $derived(
    this.status?.jobs?.upload?.state === 'running' ||
      this.status?.jobs?.install?.state === 'running',
  );
  versionMismatch = $derived(
    this.reachable && this.appVersion !== '' && this.status.version !== this.appVersion,
  );

  installedRegions = $derived(
    this.youtubeRows.filter((row) => row.status?.installed === true),
  );
  readyRegions = $derived(
    this.download0Rows.filter((row) => row.valid === true),
  );

  stepDone = $derived({
    accounts: this.accounts.some(
      (account) => account.logged_in && account.activated && account.signed_in,
    ),
    youtube: this.installedRegions.length > 0,
    y2jb: this.readyRegions.length > 0,
    updates:
      this.installedRegions.length > 0 &&
      this.installedRegions.every((row) => row.status?.updates === 'blocked'),
  });
  completed = $derived(Object.values(this.stepDone).filter(Boolean).length);
  selectedAccount = $derived(
    this.accounts.find((account) => String(account.slot) === this.accountSlot) ?? null,
  );
  accountItems = $derived(
    this.accounts.map((account) => ({ value: String(account.slot), label: account.name })),
  );

  accountsSummary = $derived(
    this.accounts
      .filter((account) => account.logged_in && account.activated && account.signed_in)
      .map((account) => account.name)
      .join(', '),
  );
  youtubeSummary = $derived(
    this.installedRegions
      .map((row) => t.youtube.regions[row.region] ?? row.region)
      .join(', '),
  );
  y2jbSummary = $derived(
    this.readyRegions
      .map((row) => t.youtube.regions[row.region] ?? row.region)
      .join(', '),
  );
  updatesSummary = $derived(this.stepDone.updates ? t.updates.blocked : '');

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

  tagFor = (region) => {
    const tag = this.download0Tags[region];
    if (tag && this.releases.some((release) => release.tag === tag)) {
      return tag;
    }
    return this.latestTag;
  };

  setTag = (region, tag) => {
    if (!region || !tag) {
      return;
    }
    this.download0Tags = { ...this.download0Tags, [region]: tag };
  };

  // Keep every region on a release that still exists after a repo change or refresh.
  ensureTags = () => {
    const latest = this.latestTag;
    const next = { ...this.download0Tags };
    let changed = false;
    for (const entry of this.regions) {
      const current = next[entry.region];
      if (!current || !this.releases.some((release) => release.tag === current)) {
        if (latest) {
          next[entry.region] = latest;
          changed = true;
        }
      }
    }
    if (changed) {
      this.download0Tags = next;
    }
  };

  titleFor = (region) => this.regions.find((entry) => entry.region === region) ?? null;

  cacheMatches = (region) => {
    const tag = this.tagFor(region);
    return !!this.cacheTag && this.cacheTag === tag;
  };

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
      this.ensureTags();
    } catch {
      this.releases = [];
      this.releasesError = true;
    } finally {
      this.releasesLoading = false;
      this.releasesLoaded = true;
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
    this.ensureTags();
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
      if (!this.status && !this.probed?.loader && !this.probed?.alive) {
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
        this.notify('connect', t.connection.scanNothingFound, false);
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

  removeDownload0File = (region) => {
    const title = this.titleFor(region);
    if (!title) {
      return;
    }
    return this.runAction('y2jb', () => removeDownload0(title.title_id));
  };

  blockUpdates = () => this.runAction('updates', blockYoutubeUpdates);

  fetchAndJoin = async (region) => {
    const entry = this.titleFor(region);
    if (!this.build || !entry) {
      return;
    }
    this.busy = 'fetch';
    this.busyRegion = entry.region;
    this.transfer = null;
    this.clearNotice('youtube');
    try {
      const result = await fetchYoutube(this.build, entry.region);
      const pkgs = (this.cache?.pkgs ?? []).filter(
        (pkg) => pkg.build !== this.build || pkg.region !== entry.region,
      );
      this.cache = {
        download0: this.cache?.download0 ?? null,
        fakesignin: this.cache?.fakesignin ?? null,
        pkgs: [
          ...pkgs,
          {
            build: this.build,
            region: entry.region,
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
      this.busyRegion = '';
      this.transfer = null;
    }
  };

  uploadAndInstall = async (region) => {
    const entry = this.titleFor(region);
    if (!this.targetIp || !this.build || !entry) {
      return;
    }
    this.busy = 'upload';
    this.busyRegion = entry.region;
    this.transfer = null;
    this.clearNotice('youtube');
    try {
      await uploadMerged(this.targetIp, this.build, entry.region);
      const result = await youtubeInstall(entry.package_digest, entry.title_id);
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
      this.busyRegion = '';
      this.transfer = null;
      await this.refresh();
      this.advance('youtube');
    }
  };

  getDownload0 = async (region) => {
    const tag = this.tagFor(region);
    if (!tag) {
      return;
    }
    this.busy = 'download0-fetch';
    this.busyRegion = region;
    this.transfer = null;
    this.clearNotice('y2jb');
    try {
      const result = await fetchDownload0(this.activeRepo, tag);
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
      this.busyRegion = '';
      this.transfer = null;
    }
  };

  sendDownload0 = async (region) => {
    const entry = this.titleFor(region);
    if (!entry || !this.cacheMatches(region)) {
      return;
    }
    this.busy = 'download0-upload';
    this.busyRegion = region;
    this.transfer = null;
    this.clearNotice('y2jb');
    try {
      await uploadDownload0(this.targetIp, entry.title_id);
      this.notify('y2jb', t.y2jb.done);
    } catch (error) {
      this.notify('y2jb', String(error), false);
    } finally {
      this.busy = '';
      this.busyRegion = '';
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

  setRepo = async (repoSlug) => {
    const slug = repoSlug
      .trim()
      .replace(/^https?:\/\/github\.com\//, "")
      .replace(/\/+$/, "");
    if (!slug || !/^[\w.-]+\/[\w.-]+$/.test(slug)) return;
    if (slug !== OFFICIAL_DOWNLOAD0_REPO && !this.customRepos.includes(slug)) {
      this.customRepos = [...this.customRepos, slug];
    }
    this.activeRepo = slug;
    this.download0Tags = {};
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
    this.busyRegion = '';
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
