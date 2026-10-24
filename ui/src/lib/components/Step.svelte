<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<script>
  import ChevronDownIcon from '@lucide/svelte/icons/chevron-down';
  import { Button } from '$lib/components/ui/button/index.js';
  import { Progress } from '$lib/components/ui/progress/index.js';
  import { strings as t } from '$lib/strings.js';
  import { formatBytes, progressPercent } from '$lib/format.js';
  import { cn } from '$lib/utils.js';
  import StepBadge from '$lib/components/StepBadge.svelte';

  let {
    id,
    number,
    title,
    summary = '',
    state = 'todo',
    open = false,
    locked = false,
    transfer = null,
    onStop = null,
    onOpen,
    children,
  } = $props();

  let progress = $derived(transfer ? progressPercent(transfer.done, transfer.total) : null);
</script>

<section
  {id}
  class={cn(
    'scroll-mt-24 rounded-xl bg-card text-card-foreground ring-1 transition-shadow',
    open ? 'ring-primary/40' : 'ring-foreground/10',
    locked && 'opacity-50',
  )}
>
  <div class="flex items-center gap-3 px-4 py-3.5">
    <button
      type="button"
      aria-expanded={open}
      disabled={locked}
      onclick={onOpen}
      class={cn(
        'flex min-w-0 items-center gap-3 text-left disabled:cursor-not-allowed',
        transfer ? 'max-w-56 shrink-0' : 'flex-1',
      )}
    >
      <StepBadge {number} {state} />
      <span class="min-w-0 flex-1">
        <span class="block text-sm font-medium">{title}</span>
        {#if !open && summary}
          <span class="mt-0.5 block truncate text-xs text-muted-foreground">{summary}</span>
        {/if}
      </span>
    </button>

    {#if transfer}
      <div class="flex min-w-0 flex-1 items-center gap-2">
        <Progress value={progress} class="h-1.5 flex-1" />
        <span class="shrink-0 whitespace-nowrap text-[11px] tabular-nums text-muted-foreground">
          {formatBytes(transfer.done)} / {formatBytes(transfer.total)}
        </span>
      </div>
      {#if onStop}
        <Button variant="outline" size="xs" class="shrink-0" onclick={onStop}>{t.common.stop}</Button>
      {/if}
    {/if}

    <button
      type="button"
      aria-expanded={open}
      aria-label={title}
      disabled={locked}
      onclick={onOpen}
      class="shrink-0 text-muted-foreground disabled:cursor-not-allowed"
    >
      <ChevronDownIcon class={cn('size-4 transition-transform', open && 'rotate-180')} />
    </button>
  </div>
  {#if open}
    <div class="border-t border-border px-4 py-4">
      {@render children()}
    </div>
  {/if}
</section>
