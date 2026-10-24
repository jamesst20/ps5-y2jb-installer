// SPDX-License-Identifier: GPL-3.0-or-later

export function formatBytes(bytes) {
  if (bytes === undefined || bytes === null) {
    return '';
  }
  return bytes >= 1e9 ? `${(bytes / 1e9).toFixed(2)} GB` : `${(bytes / 1e6).toFixed(1)} MB`;
}

export function releaseLabel(release, latestTag, t) {
  if (release.tag === latestTag) {
    return `${release.tag} · ${t.y2jb.releaseTagLatest}`;
  }
  if (release.prerelease) {
    return `${release.tag} · ${t.y2jb.releaseTagPrerelease}`;
  }
  return release.tag;
}

export function progressPercent(done, total) {
  return total > 0 ? Math.min(100, (done / total) * 100) : 0;
}
