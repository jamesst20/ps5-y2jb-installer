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
    <p class="text-xs text-muted-foreground">{t.updates.hint}</p>

    {#if installer.installedRegions.length > 0}
      <div
        class="-mx-4 overflow-hidden rounded-b-xl border-t border-border {installer.notices.updates
          ? ''
          : '-mb-4'}"
      >
        <div
          class="hidden border-b border-border bg-muted/30 px-3 py-2 text-[11px] font-medium tracking-wide text-muted-foreground uppercase sm:grid sm:grid-cols-[minmax(0,1fr)_minmax(0,1.4fr)_7rem] sm:gap-3"
        >
          <span>{t.updates.region}</span>
          <span>{t.updates.state}</span>
          <span class="text-right">{t.updates.action}</span>
        </div>

        <div class="divide-y divide-border">
          {#each installer.installedRegions as row (row.title_id)}
            <div
              class="grid grid-cols-2 gap-x-4 gap-y-2 px-3 py-3 sm:grid-cols-[minmax(0,1fr)_minmax(0,1.4fr)_7rem] sm:items-center sm:gap-3"
            >
              <div class="min-w-0">
                <span
                  class="mb-1 block text-[10px] font-medium tracking-wide text-muted-foreground uppercase sm:hidden"
                >
                  {t.updates.region}
                </span>
                <span class="block truncate text-sm font-medium">
                  {t.youtube.regions[row.region] ?? row.region}
                </span>
                <span class="block font-mono text-[10px] text-muted-foreground">{row.title_id}</span>
              </div>

              <div class="min-w-0">
                <span
                  class="mb-1 block text-[10px] font-medium tracking-wide text-muted-foreground uppercase sm:hidden"
                >
                  {t.updates.state}
                </span>
                {#if row.status?.updates === 'blocked'}
                  <Badge class="border-success/30 bg-success/10 text-success">
                    {t.updates.blocked}
                  </Badge>
                {:else}
                  <Badge variant="outline">{t.updates.notBlocked}</Badge>
                {/if}
              </div>

              <div class="col-span-2 flex items-center justify-between gap-2 sm:col-span-1 sm:justify-end">
                <span
                  class="text-[10px] font-medium tracking-wide text-muted-foreground uppercase sm:hidden"
                >
                  {t.updates.action}
                </span>
                <input
                  type="checkbox"
                  class="size-4 cursor-pointer accent-primary disabled:cursor-not-allowed disabled:opacity-50"
                  aria-label={t.updates.selectRegion(t.youtube.regions[row.region] ?? row.region)}
                  checked={installer.updateChecked(row.region)}
                  disabled={installer.busy !== ''}
                  onchange={(event) =>
                    installer.setUpdateChecked(row.region, event.currentTarget.checked)}
                />
              </div>
            </div>
          {/each}
        </div>

        <div
          class="flex flex-wrap items-center justify-between gap-2 border-t border-border bg-muted/30 px-3 py-2.5"
        >
          <span class="text-xs text-muted-foreground">
            {t.updates.selectedCount(
              installer.selectedUpdateRows.length,
              installer.installedRegions.length,
            )}
          </span>
          <Button
            size="sm"
            disabled={!installer.kernelReady ||
              installer.busy !== '' ||
              installer.selectedUpdateRows.length === 0 ||
              installer.stepDone.updates}
            onclick={() => (installer.confirmBlock = true)}
          >
            {t.updates.block}
          </Button>
        </div>
      </div>
    {:else}
      <p class="text-xs text-muted-foreground">{t.common.notInstalled}</p>
    {/if}

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
