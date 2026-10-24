# Y2JB Installer

A cross-platform application that prepares a PlayStation 5 for **Y2JB**. It handles account activation, fake PSN sign-in, YouTube installation, `download0.dat` placement, and blocking YouTube updates. It runs on Windows, macOS, Linux, and Android.

This project does not include an exploit. The console only needs kernel read/write access from a compatible exploit; once `download0.dat` is in place, launching YouTube starts Y2JB.

> **Unsigned desktop builds.** Desktop artifacts are not signed with an Apple Developer ID or a Windows code-signing certificate. macOS requires a first-launch confirmation, and Windows SmartScreen displays a warning. The Android APK is self-signed for sideloading. See [Installation](#installation).

## Overview

Y2JB Installer consists of two cooperating components:

- **Application** — a Tauri 2 / Svelte 5 application for desktop and Android that owns the interface, performs all internet access, downloads and verifies files, and uploads them to the console.
- **PS5 payload** — a small C HTTP service that runs on the console and performs the privileged operations: uploads, YouTube installation, account activation, fake sign-in, `download0.dat` placement, and `appinfo.db` patching.

The console does not need outbound internet access. The application fetches Sony packages and GitHub assets, verifies them, and transfers the results over the local network.

## Features

- Offline local account activation and fake PSN sign-in.
- Firmware-aware YouTube build selection for firmware **4.03 – 13.60**. The [Y2JB autoloader](https://github.com/itsPLK/ps5-y2jb-autoloader) does not support **11.40**.
- Verified Sony PKG downloads, package joining, and installation.
- `download0.dat` release selection with local/console version reporting and update detection.
- In-place YouTube update blocking with timestamped `appinfo.db` backups.
- LAN discovery and manual console connection.
- Local download cache with per-artifact status and cleanup.
- System-aware light and dark themes.

Each setup step is independent and can be repeated safely.

## Requirements

### Console

- PS5 on firmware **4.03 – 13.60**. The Y2JB autoloader does not support firmware **11.40**.
- Kernel read/write access from a compatible exploit.
- An ELF loader listening on TCP port **9021** (elfdr or equivalent).

### Desktop

- Windows 10 1803+ or Windows 11, with the WebView2 runtime.
- macOS 11 or later.
- Linux with glibc 2.35+ (Ubuntu 22.04 baseline), x64 or arm64, plus WebKitGTK 4.1, GTK 3, librsvg, and libappindicator.
- Internet access and a network route to the console.
- No administrator rights are required on Windows.

The AppImage is not fully self-contained; it uses the host glibc and graphical runtime
libraries. It is built against the Ubuntu 22.04 baseline.

### Android

- Android 7.0 or later; tested on Android 16 and Android 17.
- An arm64 device. The APK is universal across the included Android ABIs.
- Local network access to the console.
- Installation from unknown sources enabled for sideloading.

The Android release APK is signed with a self-signed release key, not distributed through
Google Play. Android 17 local-network access uses the legacy target-SDK behavior because
the app targets SDK 36.

## Installation

Download the appropriate artifact from the project's Releases page.

| Platform | Artifact |
|---|---|
| macOS (Intel and Apple Silicon) | `Y2JB.Installer_<version>_macos-universal.dmg` |
| Windows x64 | `y2jb-installer_<version>_windows-x64.zip` |
| Linux amd64 | `Y2JB.Installer_<version>_linux-amd64.AppImage` |
| Linux arm64 | `Y2JB.Installer_<version>_linux-aarch64.AppImage` |
| Android | `y2jb-installer_<version>_android-universal.apk` |

### macOS

Open the DMG and drag the application to **Applications**. The build is ad-hoc signed but not notarized. On first launch, right-click the application and choose **Open**, or allow it under **System Settings → Privacy & Security → Open Anyway**.

### Windows

Extract the ZIP and run `y2jb-installer.exe`. No installer or administrator rights are required. If SmartScreen appears, choose **More info → Run anyway**.

### Linux

Make the AppImage executable and run it. The AppImage requires FUSE; use `--appimage-extract-and-run` if FUSE is unavailable. The AppImage includes GitHub Releases update information and can be updated with AppImageUpdate; the matching `.zsync` file is published next to it.

### Android

Download the APK and open it on the device, allowing installation from unknown sources when prompted. The APK is self-signed and intended for sideloading. The payload API is plain HTTP on the local network, so use the app only on a trusted LAN. See [Android](docs/ANDROID.md) for build and signing details.

## Usage

1. Start the exploit and the ELF loader on the console.
2. Open the application, scan the LAN, and select the console. If only the loader answers, click **Send payload** to start the embedded payload.
3. **Accounts** — activate a local user, then run the fake sign-in; the application uploads `np-fake-signin-ps5.elf`, sends it to the loader, and removes the console-side copy.
4. **YouTube** — select the region. The application detects the firmware, selects the matching build, fetches and verifies both PKGs, joins them, uploads the result, and starts the installation. The merged PKG is deleted when the job finishes.
5. **Y2JB** — select a `download0.dat` release, fetch it, and upload it. The panel reports the release installed on the console, the release cached locally, and whether an update is available.
6. **Block updates** — confirm the dialog. The payload edits `/system_data/priv/mms/appinfo.db` in place, keeps a timestamped backup, and reboots the console. This is the final setup action; the same reboot also applies account activation and fake sign-in.
7. Disconnect the console from the internet, then launch YouTube to trigger Y2JB. Opening YouTube online can show the sign-in screen and overwrite `download0.dat`.

The payload does not survive a reboot. To perform another action after restarting, start the exploit and the ELF loader again and resend the payload from the application.

## Data and Persistence

### Application

Downloaded files and application settings are stored per user:

| Platform | Cache and downloads | Settings |
|---|---|---|
| macOS | `~/Library/Application Support/dev.y2jb.installer` | `~/Library/Application Support/dev.y2jb.installer` |
| Windows | `%APPDATA%\dev.y2jb.installer` | `%APPDATA%\dev.y2jb.installer` |
| Linux | `~/.local/share/dev.y2jb.installer` | `~/.config/dev.y2jb.installer` |
| Android | App-private storage | App-private storage |

The cache can be cleared from the application. Removing the application does not remove these directories.

### Console

- The placed file at `/user/download/PPSA01650/download0.dat` is the intended Y2JB artifact.
- Timestamped `appinfo.db` backups are kept under `/data/y2jb-installer/store`.
- Temporary uploads live under `/data/y2jb-installer/tmp` and are removed after use or by the startup cleanup.
- No installer settings, credentials, or resume state are stored on the console.

## Security Model

- The payload API listens on all interfaces on port **47291** and has **no authentication**. Use it only on a trusted local network and never expose the port to the internet.
- Every download is verified: Sony PKGs against pinned SHA-256 hashes, GitHub assets against their published digests. TLS verification is relaxed only for the pinned Sony host that requires it; the SHA-256 check remains the trust anchor.
- Uploads use fixed slots. The client cannot choose a destination path, and every upload is length- and hash-verified before it is moved into place.
- The payload performs a DNS lookup and TCP reachability probe solely to detect whether the console has effective internet access; it has no general internet client.
- YouTube updates are blocked by editing the original database in place, preserving its ownership, permissions, and inode. Backups are deliberately retained; there is no restore operation.
- The application stores no PSN credentials and sends no telemetry.

## Building from Source

### Prerequisites

- [PS5 payload SDK](https://github.com/ps5-payload-dev/sdk) with clang/lld 18.
- Node.js 24.
- Rust stable.
- GNU Make, a POSIX shell, and the platform's [Tauri prerequisites](https://v2.tauri.app/start/prerequisites/).

### Build

```sh
git submodule update --init --recursive
make test                 # host tests; no PS5 SDK required
scripts/build_deps.sh     # libmicrohttpd and SQLite for the payload
make payload              # build/y2jb-installer.elf
make app                  # UI, payload, and desktop application
```

The payload ELF is embedded in the desktop binary, so the deployed payload always matches the application that sent it.

CI resolves the latest PS5 payload SDK release at build time and verifies the ZIP against GitHub's published SHA-256 digest. Set `SDK_TAG` to pin a release when debugging.

Platform prerequisites and release builds are in [Building](docs/BUILDING.md).

## Testing

```sh
make test
npm --prefix ui run check
npm --prefix ui run build
cargo test --manifest-path app/src-tauri/Cargo.toml
```

The host and unit tests run without a console. Network-dependent tests are described in [Testing](docs/TESTING.md).

## Documentation

| Document | Contents |
|---|---|
| [Changelog](CHANGELOG.md) | Release history |
| [Android](docs/ANDROID.md) | Android build, signing, and current limitations |
| [Building](docs/BUILDING.md) | Build steps and platform-specific prerequisites |
| [Testing](docs/TESTING.md) | Test suite commands |
| [Releasing](docs/RELEASING.md) | Release steps |

## License

GPL-3.0-or-later. See [LICENSE](LICENSE).

## Credits

| Component | Use |
|---|---|
| [offact](https://github.com/ps5-payload-dev/offact) | Offline account activation; pinned as a submodule |
| [ps5-account-tools](https://github.com/MangoScango/ps5-account-tools) | Account enumeration and registry slot mapping |
| [ps5-y2jb-autoloader](https://github.com/itsPLK/ps5-y2jb-autoloader) | Y2JB runtime and `download0.dat` releases |
| [PS5 payload SDK](https://github.com/ps5-payload-dev/sdk) | Payload build toolchain |

Third-party dependencies retain their upstream licenses; see `third_party/` and the
dependency sources.
