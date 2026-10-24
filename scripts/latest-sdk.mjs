#!/usr/bin/env node
// SPDX-License-Identifier: GPL-3.0-or-later
// Resolve the newest PS5 payload SDK release for CI.
//
// The download is still integrity-checked: GitHub publishes a SHA-256 digest for each
// release asset, and the workflows verify the ZIP against it. That gives us "latest" without
// trusting whatever arrives over the wire.
//
// Set SDK_TAG to pin a release for debugging (for example SDK_TAG=v0.45). Unset means the
// latest non-prerelease release.
import { appendFileSync } from 'node:fs';

const REPO = 'ps5-payload-dev/sdk';
const ASSET = 'ps5-payload-sdk.zip';
const pinned = process.env.SDK_TAG?.trim();
const endpoint = pinned
  ? `https://api.github.com/repos/${REPO}/releases/tags/${pinned}`
  : `https://api.github.com/repos/${REPO}/releases/latest`;

const headers = {
  Accept: 'application/vnd.github+json',
  'User-Agent': 'y2jb-installer',
};
const token = process.env.GITHUB_TOKEN || process.env.GH_TOKEN;
if (token) {
  headers.Authorization = `Bearer ${token}`;
}

const response = await fetch(endpoint, { headers });
if (!response.ok) {
  throw new Error(`GitHub API ${response.status}: ${await response.text()}`);
}

const release = await response.json();
const asset = (release.assets ?? []).find((candidate) => candidate.name === ASSET);
if (!asset) {
  throw new Error(`release ${release.tag_name} has no ${ASSET} asset`);
}

const digest = String(asset.digest ?? '').toLowerCase();
const match = digest.match(/^sha256:([0-9a-f]{64})$/);
if (!match) {
  throw new Error(
    `release ${release.tag_name} has no sha256 asset digest; refusing to download it unverified`,
  );
}

const values = [
  `SDK_TAG=${release.tag_name}`,
  `SDK_URL=${asset.browser_download_url}`,
  `SDK_SHA256=${match[1]}`,
];

const githubEnv = process.env.GITHUB_ENV;
if (githubEnv) {
  appendFileSync(githubEnv, `${values.join('\n')}\n`);
}
for (const value of values) {
  console.log(value);
}
console.log(`Using ${pinned ? 'pinned' : 'the latest'} PS5 payload SDK: ${release.tag_name}`);
