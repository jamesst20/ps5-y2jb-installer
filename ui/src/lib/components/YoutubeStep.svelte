<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<script>
  import { Button } from '$lib/components/ui/button/index.js';
  import { Label } from '$lib/components/ui/label/index.js';
  import { Alert, AlertDescription } from '$lib/components/ui/alert/index.js';
  import {
    Select, SelectContent, SelectItem, SelectTrigger, SelectValue,
  } from '$lib/components/ui/select/index.js';
  import { strings as t } from '$lib/strings.js';
  import Step from '$lib/components/Step.svelte';
  import Notice from '$lib/components/Notice.svelte';

  let { installer } = $props();
</script>

<Step
  id="step-youtube"
  number="2"
  title={t.youtube.title}
  transfer={installer.youtubeTransfer}
  onStop={installer.stopTransfer}
  summary={installer.youtubeSummary}
  state={installer.stepState('youtube')}
  locked={!installer.reachable}
  open={installer.reachable && installer.openStep === 'youtube'}
  onOpen={() => installer.selectStep('youtube')}
>
  <div class="flex flex-col gap-3">
    <div class="grid gap-3 sm:grid-cols-2">
      <div class="flex flex-col gap-1">
        <span class="text-xs text-muted-foreground">{t.youtube.installed}</span>
        <span class="text-sm font-medium">
          {#if installer.installed?.installed}
            {installer.youtubeSummary}
          {:else}
            {t.common.notInstalled}
          {/if}
        </span>
      </div>
      <div class="flex flex-col gap-1.5">
        <Label for="region">{t.youtube.region}</Label>
        <Select type="single" bind:value={installer.region} items={installer.regionItems}>
          <SelectTrigger id="region" class="w-full">
            <SelectValue placeholder={t.youtube.region} />
          </SelectTrigger>
          <SelectContent>
            {#each installer.regions as entry (entry.region)}
              <SelectItem value={entry.region} label={t.youtube.regions[entry.region] ?? entry.region}>
                {t.youtube.regions[entry.region] ?? entry.region}
              </SelectItem>
            {/each}
          </SelectContent>
        </Select>
      </div>
    </div>

    {#if !installer.build}
      <Alert class="border-warning/40 bg-warning/10">
        <AlertDescription class="text-warning">{t.youtube.unsupportedFirmware}</AlertDescription>
      </Alert>
    {/if}

    <div class="flex flex-wrap gap-2">
      <Button
        disabled={!installer.desktop || !installer.build || installer.busy !== ''}
        loading={installer.busy === 'fetch'}
        onclick={installer.fetchAndJoin}
      >
        {t.youtube.fetchJoin}
      </Button>
      <Button
        variant={installer.alreadyInstalled ? 'outline' : 'default'}
        disabled={!installer.targetIp || !installer.build || installer.busy !== '' || !installer.cachedPkg?.merged || installer.jobRunning}
        loading={installer.busy === 'upload'}
        onclick={installer.uploadAndInstall}
      >
        {installer.alreadyInstalled ? t.youtube.reinstall : t.youtube.install}
      </Button>
    </div>

    <div class="flex flex-col gap-0.5">
      <p class="text-xs {installer.cachedPkg?.merged ? 'text-success' : 'text-destructive'}">
        {installer.cachedPkg?.merged ? t.youtube.cached : t.common.notCached}
      </p>
      {#if installer.cachedPkg?.path}
        <p
          class="break-all font-mono text-[11px] {installer.cachedPkg.merged
            ? 'text-success/80'
            : 'text-muted-foreground'}"
        >
          {installer.cachedPkg.path}
        </p>
      {/if}
    </div>

    {#if installer.installJob && installer.installJob.state !== 'idle'}
      <p
        class="text-sm {installer.installJob.state === 'error'
          ? 'text-destructive'
          : installer.installJob.state === 'done'
            ? 'text-success'
            : 'text-muted-foreground'}"
      >
        {installer.installJob.message}
      </p>
    {/if}

    <Notice notice={installer.notices.youtube} />
  </div>
</Step>
