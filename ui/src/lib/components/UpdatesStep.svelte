<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<script>
  import { Button } from '$lib/components/ui/button/index.js';
  import { Badge } from '$lib/components/ui/badge/index.js';
  import {
    Dialog, DialogContent, DialogDescription, DialogFooter, DialogHeader, DialogTitle,
  } from '$lib/components/ui/dialog/index.js';
  import { strings as t } from '$lib/strings.js';
  import Step from '$lib/components/Step.svelte';
  import Notice from '$lib/components/Notice.svelte';

  let { installer } = $props();
</script>

<Step
  id="step-updates"
  number="4"
  title={t.updates.title}
  summary={installer.updatesSummary}
  state={installer.stepState('updates')}
  locked={!installer.reachable}
  open={installer.reachable && installer.openStep === 'updates'}
  onOpen={() => installer.selectStep('updates')}
>
  <div class="flex flex-col gap-3">
    <div class="flex items-center justify-between text-sm">
      <span>{t.updates.hint}</span>
      <Badge variant={installer.stepDone.updates ? 'secondary' : 'outline'}>
        {installer.stepDone.updates ? t.updates.blocked : t.updates.notBlocked}
      </Badge>
    </div>
    {#if installer.installedRegions.length > 0}
      <div class="flex flex-col gap-1">
        {#each installer.installedRegions as row (row.title_id)}
          <div class="flex items-center justify-between text-xs">
            <span>
              {t.youtube.regions[row.region] ?? row.region}
              <span class="font-mono text-muted-foreground">{row.title_id}</span>
            </span>
            <span class={row.status?.updates === 'blocked' ? 'text-success' : 'text-muted-foreground'}>
              {row.status?.updates === 'blocked' ? t.updates.blocked : t.updates.notBlocked}
            </span>
          </div>
        {/each}
      </div>
    {:else}
      <p class="text-xs text-muted-foreground">{t.common.notInstalled}</p>
    {/if}
    <Button
      class="self-start"
      disabled={!installer.kernelReady || installer.busy !== '' || installer.installedRegions.length === 0 || installer.stepDone.updates}
      onclick={() => (installer.confirmBlock = true)}
    >
      {t.updates.block}
    </Button>
    <Notice notice={installer.notices.updates} />
  </div>
</Step>

<Dialog bind:open={installer.confirmBlock}>
  <DialogContent>
    <DialogHeader>
      <DialogTitle>{t.updates.confirmTitle}</DialogTitle>
      <DialogDescription>{t.updates.confirmBody}</DialogDescription>
    </DialogHeader>
    <DialogFooter>
      <Button variant="outline" onclick={() => (installer.confirmBlock = false)}>{t.common.cancel}</Button>
      <Button
        onclick={() => {
          installer.confirmBlock = false;
          installer.blockUpdates();
        }}
      >
        {t.updates.confirmProceed}
      </Button>
    </DialogFooter>
  </DialogContent>
</Dialog>
