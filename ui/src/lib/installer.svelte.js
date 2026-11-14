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
  isTransportError,
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
  clearYoutubeCache,
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
import { POLL_MS, STATUS_FAILURES_BEFORE_PAUSE, STEP_ORDER } from './constants.js';
import { normalizeRepo, progressPercent, regionName, releaseLabel } from './format.js';

export const OFFICIAL_DOWNLOAD0_REPO = 'itsPLK/ps5-y2jb-autoloader';

const INSTALL_PHASES = {
  transferring: t.youtube.installTransferring,
  promoting: t.youtube.installPromoting,
  playable: t.youtube.installFinishing,
  completed: t.youtube.installFinishing,
  error: t.youtube.installFailed,
};

const YOUTUBE_TRANSFERS = ['base', 'sc', 'merged'];

const isReadyAccount = (account) => account.logged_in && account.activated && account.signed_in;

/** Native commands reject with a string; payload calls with a fetch or timeout error. */
function errorText(error) {
  if (typeof error === 'string') return error;
  if (isTransportError(error)) return t.connection.consoleUnreachable;
  return error?.message ?? String(error);
}

/** A payload reply as a notice: its message, or the HTTP status when it has none. */
const resultText = (result) =>
  result.message ?? (result.ok ? '' : t.common.errorStatus(result.status));

export class Installer {
  appVersion = $state('');
  ip = $state('');
  found = $state([]);
  probed = $state(null);
  scanning = $state(false);
  connecting = $state(false);
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
  download0Tag = $state('');
  customRepos = $state([]);
  activeRepo = $state(OFFICIAL_DOWNLOAD0_REPO);
  transfer = $state(null);
  busy = $state('');
  busyRegion = $state('');
  rebootPending = $state(false);
  confirmBlock = $state(false);
  updateSelections = $state({});
  initialized = $state(false);
  // Nothing is polled until the user connects to the address in the field.
  paused = $state(true);
  notices = $state({});
  openStep = $state('');
  accountSlot = $state('');

  desktop = isDesktop;
  timer = null;
  unlisten = [];
  stopped = false;
  refreshing = false;
  failures = 0;
  releaseRequest = 0;

  reachable = $derived(this.status !== null);
  // A file is streaming to the console. Its progress events are proof of life, so a status
  // poll that times out meanwhile is the payload being busy, not the console going away.
  transferring = $derived(['upload', 'download0-upload', 'signin'].includes(this.busy));
  loaderVisible = $derived(!this.reachable && this.probed?.loader === true);
  consoleUp = $derived(!this.reachable && this.probed?.alive === true);
  canSendPayload = $derived(this.desktop && !!this.ip && !this.sending);
  payloadPercent = $derived(progressPercent(this.sent, this.total));
  accounts = $derived(this.status?.accounts?.list ?? []);
  readyAccounts = $derived(this.accounts.filter(isReadyAccount));
  kernelReady = $derived(this.status?.kernel_rw === true);
  regions = $derived(
    this.manifest?.builds?.find((entry) => entry.build === this.build)?.regions ?? [],
  );
  latestTag = $derived(
    this.releases.find((release) => !release.prerelease)?.tag ?? this.releases[0]?.tag ?? '',
  );
  selectedDownload0Tag = $derived(
    this.releases.some((release) => release.tag === this.download0Tag)
      ? this.download0Tag
      : this.latestTag,
  );
  releaseItems = $derived(
    this.releases.map((release) => ({
      value: release.tag,
      label: releaseLabel(release, this.latestTag),
    })),
  );

  // The cached file is identified by its real digest, matched against the release list.
  // A digest that is not listed is shown as an unknown version instead of a guess.
  cacheTag = $derived(
    (this.cache?.download0?.digest &&
      this.releases.find((release) => release.digest === this.cache.download0.digest)?.tag) ||
      '',
  );
  // The cached download0.dat is the release selected for upload.
  cacheReady = $derived(!!this.cacheTag && this.cacheTag === this.selectedDownload0Tag);

  cacheLabel = $derived.by(() => {
    if (this.cacheLoading) return t.common.computing;
    if (!this.cache?.download0) return t.y2jb.none;
    return this.cacheTag || t.y2jb.unknownVersion;
  });

  youtubeRows = $derived.by(() =>
    this.regions.map((entry) => {
      const status =
        this.status?.youtube?.find((row) => row.title_id === entry.title_id) ?? null;
      const cached =
        this.cache?.pkgs?.find((pkg) => pkg.build === this.build && pkg.region === entry.region) ??
        null;
      // Package digests are unique per build and region, so a match means this exact PKG.
      const alreadyInstalled =
        status?.installed === true &&
        !!status.package_digest &&
        status.package_digest === entry.package_digest;
      const job = this.status?.jobs?.install;
      const installJob = job?.title_id === entry.title_id && job.state !== 'idle' ? job : null;
      return { ...entry, status, cached, alreadyInstalled, installJob };
    }),
  );

  download0Rows = $derived.by(() =>
    this.regions.map((entry) => {
      const status =
        this.status?.download0?.find((row) => row.title_id === entry.title_id) ?? null;
      const youtube = this.status?.youtube?.find((row) => row.title_id === entry.title_id);
      const version = status?.version ?? '';
      const valid = status?.present === true && status.version_state === 'ready';
      // Upstream Gezine version strings such as "Y2JB 1.7 by Gezine" are not release tags,
      // so do not compare them with the repository's latest tag.
      const releaseComparable = /^[vV0-9]/.test(version);
      const installedLabel = !status?.present
        ? t.common.notInstalled
        : status.version_state === 'computing'
          ? t.common.computing
          : valid
            ? version || t.y2jb.unknownVersion
            : t.y2jb.notY2jb;
      return {
        ...entry,
        status,
        youtubeInstalled: youtube?.installed === true,
        valid,
        installedLabel,
        updateAvailable:
          valid && releaseComparable && !!this.latestTag && version !== this.latestTag,
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
  selectedUpdateRows = $derived(
    this.installedRegions.filter((row) => this.updateChecked(row.region)),
  );
  readyRegions = $derived(this.download0Rows.filter((row) => row.valid));

  stepDone = $derived({
    accounts: this.readyAccounts.length > 0,
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

  accountsSummary = $derived(this.readyAccounts.map((account) => account.name).join(', '));
  youtubeSummary = $derived(this.installedRegions.map((row) => regionName(row.region)).join(', '));
  y2jbSummary = $derived(this.readyRegions.map((row) => regionName(row.region)).join(', '));
  updatesSummary = $derived(this.stepDone.updates ? t.updates.blocked : '');

  youtubeTransfer = $derived(
    YOUTUBE_TRANSFERS.includes(this.transfer?.label) ? this.transfer : null,
  );
  download0Transfer = $derived(this.transfer?.label === 'download0' ? this.transfer : null);
  signinTransfer = $derived(this.transfer?.label === 'fakesignin' ? this.transfer : null);

  setDownload0Tag = (tag) => {
    if (tag) {
      this.download0Tag = tag;
    }
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
    clearInterval(this.timer);
    this.timer = null;
    this.unlisten.splice(0).forEach((off) => off());
  };

  // Subscribes to a native event, dropping the subscription if the app stopped meanwhile.
  listen = async (subscribe, handler) => {
    const off = await subscribe(handler);
    if (this.stopped) off();
    else this.unlisten.push(off);
  };

  loadInitialData = async () => {
    // The release list, the manifests and the cache hash are independent; start them all
    // together. cacheStatus hashes the cached image, so it is the slow one.
    this.loadReleases();
    this.listen(onPayloadProgress, (progress) => {
      this.sent = progress.sent;
      this.total = progress.total;
    });
    this.listen(onTransferProgress, (progress) => {
      this.transfer = progress;
    });
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
        this.notify('connect', errorText(error), false);
      }
    } finally {
      if (!this.stopped) {
        this.cacheLoading = false;
      }
    }
  };

  loadReleases = async () => {
    const request = ++this.releaseRequest;
    const repo = this.activeRepo;
    const isCurrent = () => request === this.releaseRequest && repo === this.activeRepo;
    this.releasesLoading = true;
    this.releasesError = false;
    this.releasesLoaded = false;
    try {
      const releases = await download0Releases(repo);
      if (!isCurrent()) return;
      this.releases = releases;
    } catch {
      if (!isCurrent()) return;
      this.releases = [];
      this.releasesError = true;
    } finally {
      if (isCurrent()) {
        this.releasesLoading = false;
        this.releasesLoaded = true;
      }
    }
  };

  notify = (step, text, ok = true) => {
    if (text) {
      this.notices = { ...this.notices, [step]: { text, ok } };
    }
  };

  clearNotice = (step) => {
    if (step in this.notices) {
      const { [step]: _, ...rest } = this.notices;
      this.notices = rest;
    }
  };

  // Shows a payload reply in the step's notice and returns whether it succeeded.
  report = (step, result) => {
    this.notify(step, resultText(result), result.ok === true);
    return result.ok === true;
  };

  // Runs one user action: marks it busy, clears the step's notice, puts any error there,
  // and always resets the busy state (and polls the console when asked).
  task = async (busy, step, action, { region = '', refresh = false } = {}) => {
    this.busy = busy;
    this.busyRegion = region;
    this.transfer = null;
    this.clearNotice(step);
    try {
      await action();
    } catch (error) {
      this.notify(step, errorText(error), false);
    } finally {
      this.busy = '';
      this.busyRegion = '';
      this.transfer = null;
      if (refresh) await this.refresh();
    }
  };

  selectStep = (step) => {
    if (this.reachable) {
      this.openStep = this.openStep === step ? '' : step;
    }
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

  // Keep the account dropdown on a real account, preferring the one logged in now.
  ensureAccount = () => {
    if (this.accounts.some((account) => String(account.slot) === this.accountSlot)) {
      return;
    }
    const preferred = this.accounts.find((account) => account.logged_in) ?? this.accounts[0];
    this.accountSlot = preferred ? String(preferred.slot) : '';
  };

  stepState = (step) =>
    this.stepDone[step] ? 'done' : this.openStep === step ? 'active' : 'todo';

  installPhase = (job) => INSTALL_PHASES[job?.console_status] ?? t.youtube.installing;

  discoveryLabel = (device) => {
    // "Connected" is only for the console the app is actually driving; a scan that found a
    // payload just means it is available.
    if (this.reachable && device.ip === this.ip) {
      return t.connection.connected;
    }
    return device.api ? t.connection.payloadLoaded : t.connection.loaderFound;
  };

  releaseLabel = (release) => releaseLabel(release, this.latestTag);

  initialize = async () => {
    if (this.initialized || !this.status) {
      return;
    }
    if (!this.manifest) {
      this.manifest = await manifestView();
    }
    this.build = (await expectedBuild(this.status.firmware)) ?? '';
    this.initialized = true;
  };

  refresh = async () => {
    if (this.paused || this.refreshing) {
      return;
    }
    const ip = this.ip;
    if (!ip) {
      this.status = null;
      this.statusError = false;
      return;
    }
    this.refreshing = true;
    setApiBase(ip);
    try {
      const status = await getStatus();
      // The address changed while this poll ran; its answer is about another console.
      if (this.ip !== ip) {
        this.status = null;
        this.statusError = false;
        return;
      }
      this.status = status;
      this.statusError = false;
      this.failures = 0;
      this.ensureAccount();
      if (this.desktop) {
        await this.initialize();
      }
    } catch {
      if (this.transferring) {
        // Keep the last known state; the next poll after the transfer catches up.
        return;
      }
      this.status = null;
      this.statusError = true;
      this.openStep = '';
      this.failures += 1;
      if (this.desktop) {
        const probed = await probeConsole(ip).catch(() => null);
        if (this.ip !== ip) return;
        this.probed = probed;
      }
      // A few failed polls in a row are normal: the console stops accepting connections for
      // 10–20 s every so often. Polling keeps trying for up to five failures (~35–45 s)
      // before it pauses and waits for the user to reconnect; until then the card reflects
      // each unanswered poll as it happens.
      if (this.failures >= STATUS_FAILURES_BEFORE_PAUSE) {
        this.paused = true;
        this.rebootPending = false;
      }
    } finally {
      this.refreshing = false;
    }
  };

  // Polls a console from scratch: after a (re)connect, or after a transfer the console just
  // acknowledged, which proves it is up even if polling gave up on it during the upload.
  resumePolling = () => {
    this.paused = false;
    this.failures = 0;
  };

  useConsole = async (address) => {
    const next = address.trim();
    if (!next) {
      return;
    }
    if (next !== this.ip) {
      this.rebootPending = false;
    }
    this.ip = next;
    this.found = [];
    setApiBase(next);
    rememberConsole(next).catch(() => {});
    this.connecting = true;
    this.resumePolling();
    this.initialized = false;
    this.probed = null;
    this.clearNotice('connect');
    try {
      await this.refresh();
      if (this.status) {
        this.focusNext();
      } else if (!this.probed?.loader && !this.probed?.alive) {
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
      this.notify('connect', errorText(error), false);
    } finally {
      this.scanning = false;
    }
  };

  send = async () => {
    const address = this.ip;
    if (!address || !this.canSendPayload) {
      return;
    }
    this.sending = true;
    this.sent = 0;
    this.total = this.payload?.size ?? 0;
    rememberConsole(address).catch(() => {});
    this.clearNotice('connect');
    try {
      const bytes = await sendPayload(address);
      this.sent = bytes;
      this.total ||= bytes;
      this.notify('connect', t.connection.waiting);
      if (!(await waitForApi(address, 60000))) {
        this.notify('connect', t.connection.consoleUnreachable, false);
        return;
      }
      this.probed = { ip: address, loader: true, api: true };
      this.resumePolling();
      this.initialized = false;
      await this.refresh();
      // waitForApi succeeded, so the payload is up. Clear "waiting" even if the immediate
      // status refresh raced with a poll; the next poll fills the state.
      this.clearNotice('connect');
      this.focusNext();
    } catch (error) {
      this.notify('connect', errorText(error), false);
    } finally {
      this.sending = false;
    }
  };

  activateSelected = () => {
    const account = this.selectedAccount;
    if (!account) {
      return;
    }
    return this.task(
      'action',
      'accounts',
      async () => {
        if (this.report('accounts', await activate(account.uid))) {
          this.rebootPending = true;
        }
      },
      { refresh: true },
    );
  };

  rebootConsole = () =>
    this.task('reboot', 'connect', async () => {
      if (this.report('connect', await reboot())) {
        this.rebootPending = false;
      }
    });

  removeDownload0File = (row) =>
    this.task(
      'action',
      'y2jb',
      async () => this.report('y2jb', await removeDownload0(row.title_id)),
      { refresh: true },
    );

  updateChecked = (region) => this.updateSelections[region] !== false;

  setUpdateChecked = (region, checked) => {
    this.updateSelections = { ...this.updateSelections, [region]: checked };
  };

  blockUpdates = () => {
    const titleIds = this.selectedUpdateRows.map((row) => row.title_id);
    if (titleIds.length === 0) {
      return;
    }
    return this.task(
      'action',
      'updates',
      async () => this.report('updates', await blockYoutubeUpdates(titleIds)),
      { refresh: true },
    );
  };

  fetchAndJoin = (row) => {
    const build = this.build;
    if (!build) {
      return;
    }
    return this.task(
      'fetch',
      'youtube',
      async () => {
        const result = await fetchYoutube(build, row.region);
        const others = (this.cache?.pkgs ?? []).filter(
          (pkg) => pkg.build !== build || pkg.region !== row.region,
        );
        const pkg = { build, region: row.region, path: result.directory, merged: true };
        this.cache = { ...this.cache, pkgs: [...others, pkg] };
      },
      { region: row.region },
    );
  };

  uploadAndInstall = (row) => {
    if (!this.ip || !this.build) {
      return;
    }
    return this.task(
      'upload',
      'youtube',
      async () => {
        await uploadMerged(this.ip, this.build, row.region);
        this.resumePolling();
        // Success is shown by the install progress; only a refusal needs a notice.
        const result = await youtubeInstall(row.package_digest, row.title_id);
        if (!result.ok) this.report('youtube', result);
      },
      { region: row.region, refresh: true },
    );
  };

  getDownload0 = () => {
    const tag = this.selectedDownload0Tag;
    if (!tag) {
      return;
    }
    return this.task('download0-fetch', 'y2jb', async () => {
      const { path, size, digest, label } = await fetchDownload0(this.activeRepo, tag);
      // The fetch already verified this digest, so publish it immediately instead of
      // hashing the same 336 MB again through cacheStatus.
      this.cache = { ...this.cache, download0: { path, size, digest } };
      this.cacheLoading = false;
      this.notify('y2jb', t.y2jb.releaseReady(label));
    });
  };

  sendDownload0 = (row) => {
    if (!this.cacheReady) {
      return;
    }
    return this.task(
      'download0-upload',
      'y2jb',
      async () => {
        await uploadDownload0(this.ip, row.title_id);
        this.resumePolling();
        this.notify('y2jb', t.y2jb.done);
      },
      { region: row.region, refresh: true },
    );
  };

  fakeSigninFlow = (account) => {
    if (!this.ip || !account) {
      return;
    }
    return this.task(
      'signin',
      'accounts',
      async () => {
        // The helper is not bundled. Always fetch it, put it on the console, then run it;
        // the payload deletes it after handing it to the loader.
        const { path, size, digest } = await fetchFakesignin();
        this.cache = { ...this.cache, fakesignin: { path, size, digest } };
        await uploadFakesignin(this.ip);
        this.resumePolling();
        await this.refresh();
        if (this.report('accounts', await fakeSignin(account.uid))) {
          this.rebootPending = true;
        }
      },
      { refresh: true },
    );
  };

  wipeCache = () =>
    this.task('cache', 'y2jb', async () => {
      await clearCache();
      this.cache = await cacheStatus();
    });

  wipeDownload0Cache = () =>
    this.task('cache', 'y2jb', async () => {
      await clearDownload0Cache();
      // Only the download0 cache changed; leave the PKG and fake-sign-in entries alone.
      this.cache = { ...this.cache, download0: null };
      this.notify('y2jb', t.y2jb.cacheDeleted);
    });

  wipeYoutubeCache = () =>
    this.task('cache', 'youtube', async () => {
      await clearYoutubeCache();
      // Every YouTube build and region is gone; the other caches are untouched.
      this.cache = { ...this.cache, pkgs: [] };
      this.notify('youtube', t.youtube.cacheDeleted);
    });

  stopTransfer = () => cancelTransfer().catch(() => {});

  setRepo = async (input) => {
    const slug = normalizeRepo(input);
    if (!slug) return;
    if (slug !== OFFICIAL_DOWNLOAD0_REPO && !this.customRepos.includes(slug)) {
      this.customRepos = [...this.customRepos, slug];
    }
    this.activeRepo = slug;
    this.download0Tag = '';
    // Invalidate the old list and its cache matches before waiting for settings.
    this.releases = [];
    const loading = this.loadReleases();
    await setDownload0Repo(slug, this.customRepos).catch(() => {});
    await loading;
  };

  disconnect = () => {
    this.paused = true;
    setApiBase('');
    this.status = null;
    this.statusError = false;
    this.probed = null;
    this.failures = 0;
    this.initialized = false;
    this.rebootPending = false;
    this.confirmBlock = false;
    this.updateSelections = {};
    this.transfer = null;
    this.busyRegion = '';
    this.accountSlot = '';
    this.notices = {};
    this.openStep = '';
  };

  stopPayload = () => {
    if (!this.ip) {
      return;
    }
    return this.task('power', 'connect', async () => {
      await shutdownPayload();
      this.disconnect();
      this.notify('connect', t.connection.payloadStopped);
    });
  };
}
