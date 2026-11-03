# Changelog

## 1.2.0 — 2026-11-03

### Added

- Debian package (`.deb`) for Debian/Ubuntu distributions.
- Option to select custom repositories for `download0.dat`.
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
