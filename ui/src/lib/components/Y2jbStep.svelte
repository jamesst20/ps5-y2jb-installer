<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<script>
  import { Button } from '$lib/components/ui/button/index.js';
  import { Label } from '$lib/components/ui/label/index.js';
  import { Badge } from '$lib/components/ui/badge/index.js';
  import {
    Select, SelectContent, SelectItem, SelectTrigger, SelectValue,
  } from '$lib/components/ui/select/index.js';
  import { strings as t } from '$lib/strings.js';
  import { formatBytes, regionName } from '$lib/format.js';
  import Step from '$lib/components/Step.svelte';
  import Notice from '$lib/components/Notice.svelte';
  import RepoCombobox from '$lib/components/RepoCombobox.svelte';
  import { OFFICIAL_DOWNLOAD0_REPO } from '$lib/installer.svelte.js';

  let { installer } = $props();

  const repoItems = $derived([
    { value: OFFICIAL_DOWNLOAD0_REPO, label: t.y2jb.defaultRepo },
    ...installer.customRepos
      .filter((repo) => repo !== OFFICIAL_DOWNLOAD0_REPO)
      .map((repo) => ({ value: repo, label: repo })),
  ]);
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
    <div class="flex flex-col gap-3 sm:flex-row sm:items-end">
      <div class="flex min-w-0 flex-1 flex-col gap-1.5">
        <Label for="repo-select">{t.y2jb.repository}</Label>
        <RepoCombobox
          id="repo-select"
          class="w-full"
          bind:value={installer.activeRepo}
          items={repoItems}
          onCommit={installer.setRepo}
        />
      </div>
      <div class="flex min-w-0 flex-1 flex-col gap-1.5">
        <Label for="release-select">{t.y2jb.release}</Label>
        <Select
          type="single"
          value={installer.selectedDownload0Tag}
          items={installer.releaseItems}
          onValueChange={installer.setDownload0Tag}
        >
          <SelectTrigger
            id="release-select"
            class="w-full"
            disabled={installer.releasesLoading || installer.releases.length === 0}
          >
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
      </div>
      <div class="flex flex-wrap gap-2 sm:shrink-0">
        <Button
          size="sm"
          variant="outline"
          disabled={!installer.desktop ||
            installer.busy !== '' ||
            !installer.selectedDownload0Tag}
          loading={installer.busy === 'download0-fetch'}
          onclick={installer.getDownload0}
        >
          {t.y2jb.fetch}
        </Button>
        <Button
          size="sm"
          variant="outline"
          disabled={installer.busy !== '' || !installer.cache?.download0}
          onclick={installer.wipeDownload0Cache}
        >
          {t.y2jb.deleteCache}
        </Button>
      </div>
    </div>

    <div class="flex flex-wrap items-center gap-x-3 gap-y-1 text-xs text-muted-foreground">
      <span>
        {t.y2jb.onPc}:
        <span class="font-medium text-foreground">{installer.cacheLabel}</span>
      </span>
      {#if installer.cache?.download0}
        <span>· {formatBytes(installer.cache.download0.size)}</span>
      {/if}
    </div>

    {#if installer.releasesLoaded && installer.releases.length === 0}
      <div class="flex flex-wrap items-center gap-2">
        <p class="text-xs text-destructive">
          {installer.releasesError ? t.y2jb.releaseListUnavailable : t.y2jb.releaseListEmpty}
        </p>
        <Button size="xs" variant="outline" onclick={installer.loadReleases}>
          {t.y2jb.refresh}
        </Button>
      </div>
    {/if}

    {#if installer.download0Rows.length > 0}
      <div
        class="-mx-4 overflow-hidden rounded-b-xl border-t border-border {installer.notices.y2jb
          ? ''
          : '-mb-4'}"
      >
        <div
          class="hidden border-b border-border bg-muted/30 px-3 py-2 text-[11px] font-medium tracking-wide text-muted-foreground uppercase sm:grid sm:grid-cols-[minmax(0,1fr)_minmax(0,1.4fr)_minmax(0,1fr)_11rem] sm:gap-3"
        >
          <span>{t.y2jb.region}</span>
          <span>{t.y2jb.state}</span>
          <span>{t.y2jb.download}</span>
          <span class="text-right">{t.y2jb.action}</span>
        </div>

        <div class="divide-y divide-border">
          {#each installer.download0Rows as row (row.region)}
            <div
              class="grid grid-cols-2 gap-x-4 gap-y-2 px-3 py-3 sm:grid-cols-[minmax(0,1fr)_minmax(0,1.4fr)_minmax(0,1fr)_11rem] sm:items-center sm:gap-3"
            >
              <div class="min-w-0">
                <span
                  class="mb-1 block text-[10px] font-medium tracking-wide text-muted-foreground uppercase sm:hidden"
                >
                  {t.y2jb.region}
                </span>
                <span class="block truncate text-sm font-medium">
                  {regionName(row.region)}
                </span>
                <span class="block font-mono text-[10px] text-muted-foreground">{row.title_id}</span>
              </div>

              <div class="min-w-0">
                <span
                  class="mb-1 block text-[10px] font-medium tracking-wide text-muted-foreground uppercase sm:hidden"
                >
                  {t.y2jb.state}
                </span>
                {#if row.valid}
                  <Badge class="max-w-full truncate border-success/30 bg-success/10 text-success">
                    {row.installedLabel}
                  </Badge>
                {:else if row.status?.present}
                  <Badge
                    variant="outline"
                    class="max-w-full truncate {row.status.version_state === 'computing'
                      ? ''
                      : 'border-warning/40 bg-warning/10 text-warning'}"
                  >
                    {row.installedLabel}
                  </Badge>
                {:else}
                  <Badge variant="outline">{t.common.notInstalled}</Badge>
                {/if}
                {#if row.updateAvailable}
                  <span class="mt-0.5 block text-[10px] text-warning">
                    {t.y2jb.updateAvailable}
                  </span>
                {/if}
                {#if !row.youtubeInstalled}
                  <span class="mt-0.5 block text-[10px] text-warning">
                    {t.y2jb.regionMissingYouTube}
                  </span>
                {/if}
              </div>

              <div class="min-w-0">
                <span
                  class="mb-1 block text-[10px] font-medium tracking-wide text-muted-foreground uppercase sm:hidden"
                >
                  {t.y2jb.download}
                </span>
                {#if installer.cacheLoading}
                  <span class="text-xs text-muted-foreground">{t.common.computing}</span>
                {:else if !installer.cache?.download0}
                  <span class="text-xs text-muted-foreground">{t.y2jb.fetchFirst}</span>
                {:else if installer.cacheReady}
                  <span class="text-xs font-medium text-success">{t.y2jb.downloaded}</span>
                {:else if !installer.cacheTag}
                  <span class="text-xs font-medium text-warning">{t.y2jb.unknownVersion}</span>
                {:else}
                  <span class="text-xs font-medium text-warning">{t.y2jb.wrongRelease}</span>
                {/if}
              </div>

              <div class="flex flex-col items-stretch gap-1 sm:items-end">
                <span
                  class="mb-1 block text-[10px] font-medium tracking-wide text-muted-foreground uppercase sm:hidden"
                >
                  {t.y2jb.action}
                </span>
                <div class="flex w-full flex-col gap-1.5 sm:w-auto sm:flex-row sm:justify-end">
                  {#if !row.status?.present || row.updateAvailable}
                    <Button
                      size="sm"
                      class="w-full sm:w-auto"
                      disabled={!installer.ip ||
                        installer.busy !== '' ||
                        installer.cacheLoading ||
                        !installer.cacheReady ||
                        !row.youtubeInstalled ||
                        installer.jobRunning}
                      loading={installer.busy === 'download0-upload' &&
                        installer.busyRegion === row.region}
                      onclick={() => installer.sendDownload0(row)}
                    >
                      {row.updateAvailable ? t.y2jb.update : t.y2jb.upload}
                    </Button>
                  {/if}
                  {#if row.status?.present}
                    <Button
                      size="sm"
                      variant="outline"
                      class="w-full sm:w-auto"
                      disabled={!installer.ip ||
                        installer.busy !== '' ||
                        installer.cacheLoading ||
                        installer.jobRunning}
                      onclick={() => installer.removeDownload0File(row)}
                    >
                      {t.y2jb.uninstall}
                    </Button>
                  {/if}
                </div>
              </div>
            </div>
          {/each}
        </div>
      </div>
    {/if}

    <Notice notice={installer.notices.y2jb} />
  </div>
</Step>
