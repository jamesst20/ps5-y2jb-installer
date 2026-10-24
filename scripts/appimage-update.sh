#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Repack the Tauri AppImage with AppImageUpdate information and generate the
# matching .zsync file. Run from the repository root after the Linux bundle.
set -euo pipefail

tool_arch="${1:?usage: appimage-update.sh <x86_64|aarch64> <amd64|aarch64>}"
asset_arch="${2:?usage: appimage-update.sh <x86_64|aarch64> <amd64|aarch64>}"

bundle="app/src-tauri/target/release/bundle/appimage"
appdir="$(find "$bundle" -maxdepth 1 -type d -name '*.AppDir' -print -quit)"
appimage="$(find "$bundle" -maxdepth 1 -type f -name "*_${asset_arch}.AppImage" -print -quit)"

if [[ -z "$appdir" || -z "$appimage" ]]; then
  echo "error: could not find the AppDir or AppImage in $bundle" >&2
  exit 1
fi

tool="${RUNNER_TEMP:-${TMPDIR:-/tmp}}/appimagetool-${tool_arch}.AppImage"
if [[ ! -x "$tool" ]]; then
  curl -fsSL --retry 3 -o "$tool" \
    "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-${tool_arch}.AppImage"
  chmod +x "$tool"
fi

version="$(cat VERSION)"
normalized="$bundle/Y2JB.Installer_${version}_linux-${asset_arch}.AppImage"
update_info="gh-releases-zsync|jamesst20|ps5-y2jb-installer|latest|Y2JB.Installer_*_linux-${asset_arch}.AppImage.zsync"

# appimagetool builds from the AppDir, so replace the generated AppImage with one
# that carries the GitHub Releases update information. Normalize the filename so it
# matches the update-info pattern and the release asset names.
rm -f "$appimage" "$normalized" "${normalized}.zsync"
ARCH="$tool_arch" APPIMAGE_EXTRACT_AND_RUN=1 \
  "$tool" --no-appstream -u "$update_info" "$appdir" "$normalized"

readelf -p .upd_info "$normalized" | grep -F "gh-releases-zsync" >/dev/null

zsyncmake \
  -u "https://github.com/jamesst20/ps5-y2jb-installer/releases/latest/download/$(basename "$normalized")" \
  -o "${normalized}.zsync" \
  "$normalized"

echo "updated ${normalized}"
echo "created ${normalized}.zsync"
