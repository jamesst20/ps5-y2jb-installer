<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<script>
  import CheckIcon from '@lucide/svelte/icons/check';
  import { cn } from '$lib/utils.js';

  let { number, state = 'todo', class: className } = $props();

  const badgeClass = {
    done: 'bg-success/15 text-success',
    active: 'bg-primary text-primary-foreground',
    todo: 'bg-muted text-muted-foreground',
  };
</script>

<span
  class={cn(
    'flex size-7 shrink-0 items-center justify-center rounded-full text-xs font-semibold',
    badgeClass[state],
    className,
  )}
>
  {#if state === 'done'}
    <CheckIcon class="size-3.5" />
  {:else}
    {number}
  {/if}
</span>
