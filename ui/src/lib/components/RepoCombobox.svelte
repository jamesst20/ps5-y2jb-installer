<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<script>
  import { Combobox } from 'bits-ui';
  import ChevronDownIcon from '@lucide/svelte/icons/chevron-down';
  import { cn } from '$lib/utils.js';
  import { strings as t } from '$lib/strings.js';

  let {
    id = undefined,
    value = $bindable(''),
    items = [],
    onCommit,
    class: className,
  } = $props();

  let open = $state(false);
  let text = $state('');

  const repoPattern = /^[\w.-]+\/[\w.-]+$/;
  const query = $derived(text.trim());
  const selectedLabel = $derived(
    items.find((item) => item.value === value)?.label ?? value,
  );
  const shown = $derived(open ? text : selectedLabel);

  const filtered = $derived.by(() => {
    const base =
      query === '' || query === value
        ? items
        : items.filter((item) => item.label.toLowerCase().includes(query.toLowerCase()));
    if (query !== '' && repoPattern.test(query) && !base.some((item) => item.value === query)) {
      return [{ value: query, label: query }, ...base];
    }
    return base;
  });

  function normalize(raw) {
    return raw
      .trim()
      .replace(/^https?:\/\/github\.com\//, '')
      .replace(/\/+$/, '');
  }

  function commit(raw) {
    const slug = normalize(raw);
    if (!slug || !repoPattern.test(slug)) return;
    if (slug !== value) onCommit?.(slug);
    open = false;
  }

  function handleValueChange(next) {
    if (!next) return;
    text = next;
    onCommit?.(next);
    open = false;
  }
</script>

<Combobox.Root
  type="single"
  bind:open
  bind:value
  {items}
  inputValue={shown}
  onValueChange={handleValueChange}
  onOpenChange={(next) => {
    if (next) text = value;
  }}
  onOpenChangeComplete={(next) => {
    if (!next) commit(text);
  }}
>
  <div class="relative">
    <Combobox.Input
      {id}
      class={cn(
        'border-input dark:bg-input/30 focus-visible:border-ring focus-visible:ring-ring/50 aria-invalid:border-destructive h-8 w-full min-w-0 rounded-lg border bg-transparent py-2 pr-8 pl-2.5 font-mono text-xs transition-colors outline-none placeholder:font-sans placeholder:text-muted-foreground focus-visible:ring-3 disabled:cursor-not-allowed disabled:opacity-50',
        className,
      )}
      placeholder={t.y2jb.repoPlaceholder}
      aria-label={t.y2jb.repository}
      oninput={(event) => {
        text = event.currentTarget.value;
        open = true;
      }}
    />
    <Combobox.Trigger
      class="absolute top-1/2 right-2 -translate-y-1/2 touch-none"
      tabindex={-1}
    >
      <ChevronDownIcon class="text-muted-foreground size-4" />
    </Combobox.Trigger>
  </div>
  <Combobox.Portal>
    <Combobox.Content
      class="bg-popover text-popover-foreground data-open:animate-in data-closed:animate-out data-closed:fade-out-0 data-open:fade-in-0 data-closed:zoom-out-95 data-open:zoom-in-95 ring-foreground/10 relative z-50 max-h-(--bits-combobox-content-available-height) w-(--bits-combobox-anchor-width) min-w-36 overflow-y-auto rounded-lg shadow-md ring-1 duration-100"
      sideOffset={4}
    >
      <Combobox.Viewport class="p-1">
        {#each filtered as item (item.value)}
          <Combobox.Item
            value={item.value}
            label={item.label}
            class="data-highlighted:bg-accent data-highlighted:text-accent-foreground relative flex w-full cursor-default items-center rounded-md py-1 pr-2 pl-2 text-xs outline-hidden select-none"
          >
            <span class="truncate font-mono">{item.label}</span>
          </Combobox.Item>
        {:else}
          <span class="block px-2 py-1.5 text-xs text-muted-foreground">
            {t.y2jb.repoNoMatch}
          </span>
        {/each}
      </Combobox.Viewport>
    </Combobox.Content>
  </Combobox.Portal>
</Combobox.Root>
