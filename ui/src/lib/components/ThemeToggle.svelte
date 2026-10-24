<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<script>
  import MoonIcon from '@lucide/svelte/icons/moon';
  import SunIcon from '@lucide/svelte/icons/sun';
  import { toggleMode } from 'mode-watcher';
  import { Button } from '$lib/components/ui/button/index.js';
  import { strings as t } from '$lib/strings.js';
</script>

<Button
  variant="ghost"
  size="icon"
  class="ml-auto"
  onclick={toggleMode}
  aria-label={t.app.toggleTheme}
  title={t.app.toggleTheme}
>
  <SunIcon class="size-4 dark:hidden" />
  <MoonIcon class="hidden size-4 dark:block" />
</Button>
