// SPDX-License-Identifier: GPL-3.0-or-later

// All user-visible text, grouped by the component that owns it. Strings shared by
// several components live in common so no component imports another component's section.
const localDevice =
  typeof navigator !== 'undefined' && /Android/i.test(navigator.userAgent) ? 'device' : 'PC';

export const strings = {
  common: {
    cancel: 'Cancel',
    stop: 'Stop',
    computing: 'Computing…',
    notInstalled: 'Not installed',
    notCached: 'Files are not fetched yet.',
    errorStatus: (status) => `Error ${status}`,
    noConsoleSelected: 'No console selected.',
    needsDesktopApp: 'This action needs the desktop app.',
  },

  app: {
    title: 'Y2JB Installer',
    subtitle: 'Console setup',
    setupComplete: 'Setup complete',
    setupCompleteHint: 'Open YouTube on the console to start Y2JB.',
    rebootRequired: 'A reboot is required',
    rebootRequiredHint: 'Account activation and sign-in take effect after restarting. You can finish the remaining steps first.',
    rebootNow: 'Reboot now',
  },

  connection: {
    console: 'Console',
    connected: 'Connected',
    notConnected: 'Not connected',
    firmware: 'Firmware',
    kernelRw: 'Kernel R&W',
    kernelOk: 'Available',
    kernelMissing: 'Unavailable',
    kernelMissingHint: 'Kernel R&W is not available. Actions are disabled.',
    payload: 'Payload',
    disconnect: 'Disconnect',
    stopPayload: 'Turn off payload',
    payloadStopped: 'Payload stopped.',

    consoleIpPlaceholder: 'Console IP',
    address: 'Address',
    connect: 'Connect',
    connecting: 'Connecting…',
    scan: 'Scan',
    scanning: 'Scanning…',
    found: 'Found on your network',
    loaderFound: 'Loader found, payload not loaded',
    payloadLoaded: 'Payload loaded',
    nothingAnswered: 'Nothing answered on',
    consoleUnreachable: 'The console did not answer.',
    sendPayload: 'Send payload',
    sending: 'Sending…',
    waiting: 'Waiting for the console…',
    payloadMismatch: (running, expected) =>
      `Payload ${running} is running. This app expects ${expected}.`,
  },

  setup: {
    title: 'Setup',
    done: 'Done',
    todo: 'Pending',
    clearCache: 'Clear cache',
  },

  accounts: {
    title: 'Accounts',
    account: 'Account',
    noAccounts: 'No accounts on this console.',
    loggedIn: 'Logged in',
    loggedOut: 'Logged out',
    activated: 'Activated',
    notActivated: 'Not activated',
    signedIn: 'Signed in',
    notSignedIn: 'Not signed in',
    activate: 'Activate',
    fakeSignin: 'Fake sign-in',
    logInFirst: 'Log in as this user on the console first.',
  },

  youtube: {
    title: 'YouTube',
    installed: 'Installed',
    region: 'Region',
    regions: { usa: 'USA', europe: 'Europe', japan: 'Japan' },
    fetchJoin: 'Fetch and join',
    install: 'Install',
    reinstall: 'Reinstall',
    cached: `Files are cached on this ${localDevice}.`,
    unsupportedFirmware: 'This firmware is not supported.',
  },

  y2jb: {
    title: 'Y2JB',
    onPs5: 'On the PS5',
    onPc: `Cached on this ${localDevice}`,
    unknownVersion: 'Version unknown',
    none: 'None',
    release: 'Release',
    releaseTagLatest: 'latest',
    releaseTagPrerelease: 'pre-release',
    refresh: 'Refresh',
    latest: 'Latest',
    prerelease: 'Pre-release',
    updateAvailable: 'Update available',
    upToDate: 'Up to date',
    published: 'Published',
    fetch: 'Fetch',
    upload: 'Upload',
    uninstall: 'Uninstall',
    deleteCache: 'Delete cache',
    cacheDeleted: 'The cached download0.dat was deleted.',
    cacheUnknownRelease: (tag) =>
      `The cached file is not one of the listed releases. Fetch ${tag} to replace it.`,
    cacheMismatch: (cached, selected) => `Cached ${cached}; fetch ${selected} before uploading.`,
    releaseReady: (label) => `${label} is ready.`,
    releaseListUnavailable: 'Could not read the release list from GitHub.',
    done: 'download0.dat is in place.',
    internetDetected:
      'Internet access detected on the console. Disconnect it before opening YouTube; an online launch can overwrite download0.dat and require reinstalling it.',
  },

  updates: {
    title: 'Block updates',
    hint: 'Stops YouTube from updating itself. This is the final step and restarts the console.',
    blocked: 'Blocked',
    notBlocked: 'Not blocked',
    block: 'Block updates',
    confirmTitle: 'Block YouTube updates?',
    confirmBody: 'The console will restart afterwards.',
    confirmProceed: 'Block and restart',
  },
};
