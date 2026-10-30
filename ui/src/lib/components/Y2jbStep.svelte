<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<script>
  import { Button } from '$lib/components/ui/button/index.js';
  import { Input } from '$lib/components/ui/input/index.js';
  import { Label } from '$lib/components/ui/label/index.js';
  import { Badge } from '$lib/components/ui/badge/index.js';
  import {
    Select, SelectContent, SelectItem, SelectTrigger, SelectValue,
  } from '$lib/components/ui/select/index.js';
  import { strings as t } from '$lib/strings.js';
  import { formatBytes } from '$lib/format.js';
  import Step from '$lib/components/Step.svelte';
  import Notice from '$lib/components/Notice.svelte';

  let { installer } = $props();
  let newRepoInput = $state('');

  const repoItems = $derived([
    { value: 'itsPLK/ps5-y2jb-autoloader', label: t.y2jb.defaultRepo },
    ...installer.customRepos.map((r) => ({ value: r, label: r })),
  ]);

  function handleAddRepo() {
    if (!newRepoInput.trim()) return;
    installer.addCustomRepo(newRepoInput);
    newRepoInput = '';
  }
</script>

<Step
  id="step-y2jb"
  number="3"
  title={t.y2jb.title}
  transfer={installer.download0Transfer}
  onStop={installer.stopTransfer}
  summary={installer.y2jbSummary}
  state={installer.stepState('y2jb')}
  locked={!installer.reachable}
  open={installer.reachable && installer.openStep === 'y2jb'}
  onOpen={() => installer.selectStep('y2jb')}
>
  <div class="flex flex-col gap-4">
    <div class="grid gap-3 sm:grid-cols-2">
      <div class="flex flex-wrap items-start justify-between gap-3">
        <div class="flex min-w-0 flex-col gap-1">
          <span class="text-xs text-muted-foreground">{t.y2jb.onPs5}</span>
          <span class="flex flex-wrap items-center gap-2 text-sm font-medium">
            {installer.installedLabel}
            {#if installer.status.files?.download0?.present}
              {#if installer.updateAvailable}
                <Badge class="border-warning/30 bg-warning/10 text-warning">{t.y2jb.updateAvailable}</Badge>
              {:else if installer.installedVersion && installer.installedVersion === installer.latestTag}
                <Badge class="border-success/30 bg-success/10 text-success">{t.y2jb.upToDate}</Badge>
              {/if}
            {/if}
          </span>
          <span class="break-all font-mono text-xs text-muted-foreground">
            {installer.status.files?.download0?.path}
            {#if installer.status.files?.download0?.present}
              · {formatBytes(installer.status.files.download0.size)}
            {/if}
          </span>
        </div>
        {#if installer.status.files?.download0?.present}
          <Button
            size="xs"
            variant="outline"
            class="shrink-0"
            disabled={!installer.targetIp || installer.busy !== '' || installer.jobRunning}
            onclick={installer.removeDownload0File}
          >
            {t.y2jb.uninstall}
          </Button>
        {/if}
      </div>

      <div class="flex flex-wrap items-start justify-between gap-3">
        <div class="flex min-w-0 flex-col gap-1">
          <span class="text-xs text-muted-foreground">{t.y2jb.onPc}</span>
          <span class="text-sm font-medium">{installer.cacheLabel}</span>
          <span class="text-xs text-muted-foreground">
            {#if installer.cache?.download0}
              {formatBytes(installer.cache.download0.size)}
            {:else if !installer.cacheLoading}
              {t.common.notCached}
            {/if}
          </span>
        </div>
        {#if installer.cache?.download0}
          <Button
            size="xs"
            variant="outline"
            class="shrink-0"
            disabled={installer.busy !== ''}
            onclick={installer.wipeDownload0Cache}
          >
            {t.y2jb.deleteCache}
          </Button>
        {/if}
      </div>
    </div>

    <div class="flex flex-col gap-2 rounded-lg border border-border p-3">
      <Label for="repo-select">{t.y2jb.repository}</Label>
      <div class="flex flex-wrap gap-2">
        <Select
          type="single"
          value={installer.activeRepo}
          onValueChange={(val) => installer.switchRepo(val)}
          items={repoItems}
        >
          <SelectTrigger id="repo-select" class="w-full sm:w-80">
            <SelectValue placeholder={t.y2jb.repository} />
          </SelectTrigger>
          <SelectContent>
            {#each repoItems as item (item.value)}
              <SelectItem value={item.value} label={item.label}>
                {item.label}
              </SelectItem>
            {/each}
          </SelectContent>
        </Select>
      </div>

      <div class="mt-1 flex flex-wrap gap-2">
        <Input
          class="max-w-xs font-mono text-xs"
          placeholder={t.y2jb.repoPlaceholder}
          bind:value={newRepoInput}
          onkeydown={(e) => { if (e.key === 'Enter') handleAddRepo(); }}
        />
        <Button size="sm" variant="outline" onclick={handleAddRepo}>
          {t.y2jb.addRepo}
        </Button>
      </div>
    </div>

    <div class="flex flex-col gap-1.5">
      <Label for="release">{t.y2jb.release}</Label>
      <div class="flex flex-wrap gap-2">
        <Select type="single" bind:value={installer.selectedTag} items={installer.releaseItems}>
          <SelectTrigger id="release" class="w-full sm:w-64">
            <SelectValue placeholder={t.y2jb.release} />
          </SelectTrigger>
          <SelectContent>
            {#each installer.releases as release (release.tag)}
              <SelectItem value={release.tag} label={installer.releaseLabel(release)}>
                {installer.releaseLabel(release)}
              </SelectItem>
            {/each}
          </SelectContent>
        </Select>
        <Button
          size="sm"
          variant="outline"
          disabled={installer.releasesLoading}
          loading={installer.releasesLoading}
          onclick={installer.loadReleases}
        >
          {t.y2jb.refresh}
        </Button>
      </div>
      {#if installer.releasesError}
        <p class="text-xs text-destructive">{t.y2jb.releaseListUnavailable}</p>
      {/if}
    </div>

    {#if installer.selectedRelease}
      <div class="flex flex-wrap items-center gap-2 text-xs text-muted-foreground">
        <span>{formatBytes(installer.selectedRelease.size)}</span>
        <span>·</span>
        <span>{t.y2jb.published} {installer.selectedRelease.published_at.slice(0, 10)}</span>
      </div>
    {/if}

    <div class="flex flex-wrap gap-2">
      <Button
        size="sm"
        disabled={!installer.selectedTag || installer.busy !== ''}
        loading={installer.busy === 'download0-fetch'}
        onclick={installer.getDownload0}
      >
        {t.y2jb.fetch}
      </Button>
      <Button
        size="sm"
        variant="outline"
        disabled={!installer.targetIp || !installer.cacheMatchesSelection || installer.busy !== '' || installer.jobRunning}
        loading={installer.busy === 'download0-upload'}
        onclick={installer.sendDownload0}
      >
        {t.y2jb.upload}
      </Button>
    </div>

    <Notice notice={installer.notices.y2jb} />
  </div>
</Step>
