<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<script>
  import { Button } from '$lib/components/ui/button/index.js';
  import { Card, CardContent } from '$lib/components/ui/card/index.js';
  import { strings as t } from '$lib/strings.js';
  import { cn } from '$lib/utils.js';
  import StepBadge from '$lib/components/StepBadge.svelte';

  let { installer } = $props();

  let steps = $derived([
    {
      id: 'accounts',
      number: 1,
      title: t.accounts.title,
      summary: installer.accountsSummary,
      state: installer.stepState('accounts'),
    },
    {
      id: 'youtube',
      number: 2,
      title: t.youtube.title,
      summary: installer.youtubeSummary,
      state: installer.stepState('youtube'),
    },
    {
      id: 'y2jb',
      number: 3,
      title: t.y2jb.title,
      summary: installer.y2jbSummary,
      state: installer.stepState('y2jb'),
    },
    {
      id: 'updates',
      number: 4,
      title: t.updates.title,
      summary: installer.updatesSummary,
      state: installer.stepState('updates'),
    },
  ]);

  function detail(step) {
    if (step.summary) {
      return step.summary;
    }
    if (step.state === 'done') {
      return t.setup.done;
    }
    return t.setup.todo;
  }
</script>

<Card class="gap-0 py-0">
  <CardContent class="flex flex-col gap-4 px-4 py-4">
    <div class="flex items-center justify-between gap-3">
      <p class="text-sm font-medium">{t.setup.title}</p>
      <Button
        variant="ghost"
        size="sm"
        class="text-muted-foreground"
        disabled={installer.busy !== ''}
        onclick={installer.wipeCache}
      >
        {t.setup.clearCache}
      </Button>
    </div>

    <ol class="grid grid-cols-4">
      {#each steps as step, index (step.id)}
        <li class="relative flex flex-col items-center text-center">
          {#if index < steps.length - 1}
            <span
              aria-hidden="true"
              class={cn(
                'absolute left-[calc(50%+1rem)] top-3.5 h-px w-[calc(100%-2rem)]',
                step.state === 'done' ? 'bg-success/40' : 'bg-border',
              )}
            ></span>
          {/if}

          <button
            type="button"
            aria-current={step.state === 'active' ? 'step' : undefined}
            disabled={!installer.reachable}
            onclick={() => installer.goToStep(step.id)}
            class="relative z-10 flex w-full flex-col items-center gap-1.5 rounded-lg px-1 py-1 transition-colors hover:bg-muted/50 disabled:cursor-not-allowed disabled:opacity-50"
          >
            <StepBadge number={step.number} state={step.state} />
            <span class="w-full truncate text-xs font-medium sm:text-sm">{step.title}</span>
            <span class="w-full truncate text-[11px] text-muted-foreground">{detail(step)}</span>
          </button>
        </li>
      {/each}
    </ol>
  </CardContent>
</Card>
