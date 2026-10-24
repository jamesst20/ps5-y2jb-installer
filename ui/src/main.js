// SPDX-License-Identifier: GPL-3.0-or-later
import './app.css';
import { mount } from 'svelte';
import App from './App.svelte';

// Set the initial mode before mounting. mode-watcher normally injects an inline
// script for this, but the Tauri CSP blocks inline scripts; the storage key is shared.
const storedMode = localStorage.getItem('mode-watcher-mode') ?? 'system';
const dark =
  storedMode === 'dark' ||
  (storedMode === 'system' && window.matchMedia('(prefers-color-scheme: dark)').matches);
document.documentElement.classList.toggle('dark', dark);

mount(App, { target: document.getElementById('app') });
