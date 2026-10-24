<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<script>
  import RotateCwIcon from '@lucide/svelte/icons/rotate-cw';
  import { Button } from '$lib/components/ui/button/index.js';
  import { strings as t } from '$lib/strings.js';

  let { installer } = $props();
</script>

{#if installer.rebootPending}
  <div class="pointer-events-none fixed inset-x-0 bottom-0 z-30 px-5 pb-5">
    <div
      class="pointer-events-auto mx-auto flex max-w-3xl flex-col gap-3 rounded-xl border border-warning/40 bg-card/95 px-4 py-3 shadow-lg backdrop-blur sm:flex-row sm:items-center"
      role="status"
      aria-live="polite"
    >
      <div class="flex items-start gap-3">
        <RotateCwIcon class="mt-0.5 size-4 shrink-0 text-warning" />
        <div>
          <p class="text-sm font-medium">{t.app.rebootRequired}</p>
          <p class="text-xs text-muted-foreground">{t.app.rebootRequiredHint}</p>
        </div>
      </div>
      <Button
        class="sm:ml-auto"
        size="sm"
        variant="outline"
        loading={installer.busy === 'reboot'}
        disabled={installer.busy !== '' || installer.jobRunning || installer.transfer !== null || installer.sending}
        onclick={installer.rebootConsole}
      >
        {t.app.rebootNow}
      </Button>
    </div>
  </div>
{/if}
