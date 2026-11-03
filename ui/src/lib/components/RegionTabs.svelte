<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<script>
  import { Tabs, TabsList, TabsTrigger } from '$lib/components/ui/tabs/index.js';
  import CircleCheckIcon from '@lucide/svelte/icons/circle-check';
  import CircleXIcon from '@lucide/svelte/icons/circle-x';

  let { value = '', items = [], label = 'Region', onValueChange } = $props();
</script>

<Tabs
  {value}
  class="gap-0"
  onValueChange={(next) => {
    if (next) onValueChange?.(next);
  }}
>
  <TabsList aria-label={label} class="w-full">
    {#each items as item (item.value)}
      <TabsTrigger value={item.value} class="gap-2 text-sm">
        {#if item.done}
          <CircleCheckIcon class="text-success size-4 shrink-0" strokeWidth={2.5} />
        {:else}
          <CircleXIcon class="text-destructive size-4 shrink-0" strokeWidth={2.5} />
        {/if}
        {item.label}
      </TabsTrigger>
    {/each}
  </TabsList>
</Tabs>
