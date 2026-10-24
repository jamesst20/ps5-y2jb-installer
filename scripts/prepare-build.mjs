#!/usr/bin/env node
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Builds the UI and payload before Tauri compiles them in. This is Tauri's
// beforeBuildCommand and npm's predev hook. It must work on macOS, Linux and Windows, so
// it is a Node script rather than a shell pipeline.
//
// Y2JB_PAYLOAD_PREBUILT=1 skips `make payload` and requires an existing ELF. CI uses that
// after building the payload in a previous step.
import { spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { existsSync, readFileSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const elf = join(root, 'build', 'y2jb-installer.elf');
const ui = join(root, 'ui');
const truthy = new Set(['1', 'true', 'yes']);
const prebuilt = truthy.has(String(process.env.Y2JB_PAYLOAD_PREBUILT ?? '').toLowerCase());

function run(command, args, cwd) {
  const result = spawnSync(command, args, {
    cwd,
    stdio: 'inherit',
    shell: process.platform === 'win32',
  });
  if (result.error) {
    throw result.error;
  }
  if (result.status !== 0) {
    process.exit(result.status ?? 1);
  }
}

// The UI is cheap to rebuild and Tauri needs ui/dist to exist before it compiles.
if (!existsSync(join(ui, 'node_modules'))) {
  run('npm', ['ci'], ui);
}
run('npm', ['run', 'build'], ui);

if (prebuilt) {
  if (!existsSync(elf)) {
    console.error(`error: Y2JB_PAYLOAD_PREBUILT is set but ${elf} does not exist`);
    console.error('build the payload first, or unset the variable to let this script run make');
    process.exit(1);
  }
} else {
  run('make', ['payload'], root);
}

const payload = readFileSync(elf);
if (
  payload.length < 4 ||
  payload[0] !== 0x7f ||
  payload.subarray(1, 4).toString('ascii') !== 'ELF'
) {
  console.error(`error: ${elf} is not an ELF file`);
  process.exit(1);
}

const digest = createHash('sha256').update(payload).digest('hex');
const mebibytes = (payload.length / (1024 * 1024)).toFixed(1);
console.log(`payload: ${mebibytes} MiB, sha256 ${digest}`);
