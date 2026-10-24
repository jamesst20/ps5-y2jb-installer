#!/usr/bin/env node
// SPDX-License-Identifier: GPL-3.0-or-later
// One version number for the whole project.
//
// VERSION is the source of truth. The payload header is generated from it by the Makefile;
// this script keeps the app manifests in sync and fails the build when they drift.
//
//   node scripts/version.mjs --check   verify every copy (default)
//   node scripts/version.mjs --write   update every copy after editing VERSION
import { readFileSync, writeFileSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const versionFile = join(root, 'VERSION');
const SEMVER = /^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$/;

const version = readFileSync(versionFile, 'utf8').trim();
if (!SEMVER.test(version)) {
  console.error(`VERSION must be a plain semantic version such as 1.0.0, got "${version}"`);
  process.exit(1);
}

// Each target returns the version it currently declares and can replace it.
const targets = [
  {
    name: 'app/src-tauri/Cargo.toml',
    read(text) {
      const section = packageSection(text);
      const match = section.match(/^version = "([^"]+)"/m);
      if (!match) throw new Error('no version in [package]');
      return match[1];
    },
    write(text, next) {
      const section = packageSection(text);
      const updated = section.replace(/^version = "[^"]+"/m, `version = "${next}"`);
      return text.replace(section, updated);
    },
  },
  jsonTarget('app/src-tauri/tauri.conf.json'),
  jsonTarget('app/package.json'),
  jsonTarget('ui/package.json'),
  lockTarget('app/package-lock.json'),
  lockTarget('ui/package-lock.json'),
  cargoLockTarget('app/src-tauri/Cargo.lock'),
];

// Cargo's [package] table, so a dependency version can never be mistaken for ours.
function packageSection(text) {
  const start = text.indexOf('[package]');
  if (start === -1) throw new Error('no [package] section');
  const end = text.indexOf('\n[', start + 1);
  return end === -1 ? text.slice(start) : text.slice(start, end);
}

function jsonTarget(name) {
  return {
    name,
    read(text) {
      const parsed = JSON.parse(text);
      if (!parsed.version) throw new Error('no version field');
      return parsed.version;
    },
    write(text, next) {
      const parsed = JSON.parse(text);
      parsed.version = next;
      return `${JSON.stringify(parsed, null, 2)}\n`;
    },
  };
}

// Cargo records our package version in its own [[package]] block.
function cargoLockTarget(name) {
  const block = /(\[\[package\]\]\r?\nname = "y2jb-installer"\r?\nversion = ")([^"]+)(")/;
  return {
    name,
    read(text) {
      const match = text.match(block);
      if (!match) throw new Error('no y2jb-installer package block');
      return match[2];
    },
    write(text, next) {
      if (!block.test(text)) throw new Error('no y2jb-installer package block');
      return text.replace(block, `$1${next}$3`);
    },
  };
}

// npm records the root package version twice in package-lock.json.
function lockTarget(name) {
  return {
    name,
    read(text) {
      const parsed = JSON.parse(text);
      return parsed.packages?.['']?.version ?? parsed.version;
    },
    write(text, next) {
      const parsed = JSON.parse(text);
      parsed.version = next;
      parsed.packages[''].version = next;
      return `${JSON.stringify(parsed, null, 2)}\n`;
    },
  };
}

const write = process.argv.includes('--write');
const unknown = process.argv.slice(2).filter((arg) => arg !== '--write' && arg !== '--check');
if (unknown.length > 0) {
  console.error(`unknown argument: ${unknown.join(' ')}`);
  process.exit(2);
}

let failed = false;
for (const target of targets) {
  const path = join(root, target.name);
  const text = readFileSync(path, 'utf8');
  const current = target.read(text);
  if (current === version) {
    console.log(`ok      ${target.name}  ${current}`);
    continue;
  }
  if (write) {
    writeFileSync(path, target.write(text, version));
    console.log(`updated ${target.name}  ${current} -> ${version}`);
    continue;
  }
  console.error(`mismatch ${target.name}  ${current} != ${version}`);
  failed = true;
}

// A release tag must match VERSION. Running locally without a tag is fine.
const tag = process.env.GITHUB_REF_NAME;
if (tag && tag.startsWith('v') && tag !== `v${version}`) {
  console.error(`mismatch release tag  ${tag} != v${version}`);
  failed = true;
}

if (failed) {
  console.error('run: node scripts/version.mjs --write');
  process.exit(1);
}
