# Changelog

## Unreleased

### Added

- Experimental Android target. The full workflow is verified on Pixel 9 with Android 17; connection and payload sending are verified on OnePlus 13 with Android 16.

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
