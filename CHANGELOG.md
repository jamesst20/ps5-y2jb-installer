# Changelog

## Unreleased

### Fixed

- The app now waits out the 10–20 s windows in which a jailbroken console stops accepting TCP connections (every process on it, not only the payload). Payload sending and uploads retry the connection for up to 45 s, actions wait for the payload to answer before they are sent once, the status poll tolerates about 35 s of silence before it gives up, and a retried YouTube install for the same region is reported as already running instead of failing.
- Uploads no longer end with "the console closed the connection without replying" on fast links: the app no longer half-closes the socket after the body, which made the payload's HTTP server drop the reply.
- The console no longer shows as disconnected while a large upload is in progress. The payload now serves each request on its own thread, so a stalled disk write during an upload cannot block `/api/status`; the app ignores status timeouts while a transfer it is driving is still streaming, resumes polling once the console acknowledges the upload, waits up to 180 s for the final upload reply, and gives a manual connection probe 1.5 s instead of 250 ms.

## 1.3.0 — 2026-11-03

### Added

- Debian packages (`.deb`) for Debian/Ubuntu on amd64 and arm64.
- Option to select custom repositories for `download0.dat`.
- YouTube setup is now per region (USA `PPSA01650`, Europe `PPSA01651`, Japan `PPSA01652`), with independent install state, `download0.dat` release selection, upload, and removal for each region.
- YouTube installation now reports the console install phase, byte progress, and Sony failure descriptions instead of assuming the install succeeds.

### Changed

- Setup steps no longer close automatically after an action, so multiple regions can be configured without reopening the step.
- The Send payload button stays disabled until Connect has been used for the entered console IP.
- The repository picker keeps showing the selected repository's label instead of switching to its slug.
- The console internet warning is now shown as a red alert.

### Fixed

- The YouTube step now shows a computing state while the cache scan is still running instead of claiming no files are cached.
- Fake sign-in upload failures now report the payload's actual reason instead of a blank `fakesignin:` message.
- Connection and scan messages now distinguish an unreachable console from a reachable console with elfldr stopped, and explain the ports involved: elfldr on 9021 and the installer payload on 47291.
- A `download0.dat` created by YouTube is no longer treated as a Y2JB setup; a region is only marked ready when the image contains an `autoloader_version` or `version_string` marker.

## 1.2.0 — 2026-11-03

### Added

- Signed universal Android APK in release artifacts.
- Android build documentation and signing instructions.

### Changed

- Release CI now builds the PS5 payload once and shares it across macOS, Linux, Windows, and Android jobs.
- The release workflow can refresh assets on an existing release without moving its tag.

### Fixed

- **Firmware 13.40–13.60 installs are fixed.** The installer now selects YouTube `v01.000.030` instead of `v01.009.253` on these firmwares. On affected consoles `v01.009.253` booted straight to YouTube's Welcome screen, skipped the cached Y2JB splash, and left `download0.dat` unloaded, so the jailbreak never started. `v01.000.030` only requires FW 12.20 and still triggers the splash. `v01.009.253` remains pinned in the manifest for manual use.
- Firmware 11.40 is now supported. The autoloader gained the missing Relapse offsets in `v1.0.4`, so 11.40 uses the normal `v01.000.003` mapping instead of being listed as unsupported.

## 1.1.0 — 2026-10-24

### Added

- Experimental Android target. The full workflow is verified on Pixel 9 with Android 17; connection and payload sending are verified on OnePlus 13 with Android 16.
- System-aware light and dark themes.
- AppImage releases now include GitHub Releases update information and a matching `.zsync` file for AppImageUpdate.
- Compact scaling for narrow mobile viewports.

### Changed

- Documented the Linux glibc and WebKitGTK/GTK runtime baseline.

## 1.0.1 — 2026-10-23

### Changed

- Clarified reboot messages: account activation and fake sign-in can be completed with the remaining setup steps, then applied by the single reboot after blocking updates.
- Replaced the inline reboot hint with a persistent status bar that offers an optional **Reboot now** action.
- Success notices are green and failure notices are red.
- Normalized payload result messages to start with a capital letter.

### Fixed

- The Send payload card appears as soon as the loader is found instead of waiting for the cached `download0.dat` hash.

## 1.0.0

- Initial release.
