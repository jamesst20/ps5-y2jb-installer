<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<script>
  import { Button } from '$lib/components/ui/button/index.js';
  import { Label } from '$lib/components/ui/label/index.js';
  import { Badge } from '$lib/components/ui/badge/index.js';
  import { Alert, AlertDescription } from '$lib/components/ui/alert/index.js';
  import {
    Select, SelectContent, SelectItem, SelectTrigger, SelectValue,
  } from '$lib/components/ui/select/index.js';
  import { strings as t } from '$lib/strings.js';
  import { formatBytes } from '$lib/format.js';
  import Step from '$lib/components/Step.svelte';
  import Notice from '$lib/components/Notice.svelte';
  import RepoCombobox from '$lib/components/RepoCombobox.svelte';
  import RegionTabs from '$lib/components/RegionTabs.svelte';
  import { OFFICIAL_DOWNLOAD0_REPO } from '$lib/installer.svelte.js';

  let { installer } = $props();

  let selectedRegion = $state('');

  const repoItems = $derived([
    { value: OFFICIAL_DOWNLOAD0_REPO, label: t.y2jb.defaultRepo },
    ...installer.customRepos
      .filter((repo) => repo !== OFFICIAL_DOWNLOAD0_REPO)
      .map((repo) => ({ value: repo, label: repo })),
  ]);

  const tabs = $derived(
    installer.download0Rows.map((row) => ({
      value: row.region,
      label: t.youtube.regions[row.region] ?? row.region,
      status: !row.youtubeInstalled ? 'no-app' : row.valid === true ? 'done' : 'missing',
    })),
  );
  const row = $derived(
    installer.download0Rows.find((entry) => entry.region === selectedRegion) ??
      installer.download0Rows[0] ??
      null,
  );
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
    <div class="flex flex-wrap items-end justify-between gap-3">
      <div class="flex min-w-0 flex-col gap-1">
        <Label for="repo-select">{t.y2jb.repository}</Label>
        <RepoCombobox
          id="repo-select"
          class="sm:w-64"
          bind:value={installer.activeRepo}
          items={repoItems}
          onCommit={installer.setRepo}
        />
      </div>
      <div class="flex min-w-0 flex-col gap-1">
        <span class="text-xs text-muted-foreground">{t.y2jb.onPc}</span>
        <span class="text-sm font-medium">
          {installer.cacheLabel}
          {#if installer.cache?.download0}
            <span class="text-xs font-normal text-muted-foreground">
              · {formatBytes(installer.cache.download0.size)}
            </span>
          {/if}
        </span>
      </div>
      <div class="flex flex-wrap gap-2">
        <Button
          size="sm"
          variant="outline"
          disabled={installer.releasesLoading}
          loading={installer.releasesLoading}
          onclick={installer.loadReleases}
        >
          {t.y2jb.refresh}
        </Button>
        {#if installer.cache?.download0}
          <Button
            size="sm"
            variant="outline"
            disabled={installer.busy !== ''}
            onclick={installer.wipeDownload0Cache}
          >
            {t.y2jb.deleteCache}
          </Button>
        {/if}
      </div>
    </div>

    {#if installer.releasesError}
      <p class="text-xs text-destructive">{t.y2jb.releaseListUnavailable}</p>
    {:else if installer.releasesLoaded && !installer.releasesLoading && installer.releases.length === 0}
      <p class="text-xs text-destructive">{t.y2jb.releaseListEmpty}</p>
    {/if}

    {#if tabs.length > 0}
      <RegionTabs
        value={row?.region ?? ''}
        items={tabs}
        label={t.youtube.region}
        onValueChange={(region) => (selectedRegion = region)}
      />
    {/if}

    {#if row}
      <div class="flex flex-col gap-3">
        <div class="flex flex-wrap items-center gap-2">
          <Badge variant="outline" class="font-mono">{row.title_id}</Badge>
          {#if row.valid}
            <Badge class="border-success/30 bg-success/10 text-success">
              {row.installedLabel}
            </Badge>
            {#if row.updateAvailable}
              <Badge class="border-warning/30 bg-warning/10 text-warning">
                {t.y2jb.updateAvailable}
              </Badge>
            {:else if row.upToDate}
              <Badge class="border-success/30 bg-success/10 text-success">
                {t.y2jb.upToDate}
              </Badge>
            {/if}
          {:else if row.status?.present}
            <Badge
              variant="outline"
              class={row.status.version_state === 'computing'
                ? ''
                : 'border-warning/40 bg-warning/10 text-warning'}
            >
              {row.installedLabel}
            </Badge>
          {:else}
            <Badge variant="outline">{t.common.notInstalled}</Badge>
          {/if}
        </div>

        <span class="break-all font-mono text-xs text-muted-foreground">
          {row.status?.path}
          {#if row.status?.present && row.status.size >= 0}
            · {formatBytes(row.status.size)}
          {/if}
        </span>

        {#if row.status?.present && !row.valid && row.status.version_state !== 'computing'}
          <Alert class="border-warning/40 bg-warning/10">
            <AlertDescription class="text-warning">
              {t.y2jb.notY2jbHint}
            </AlertDescription>
          </Alert>
        {/if}

        {#if !row.youtubeInstalled}
          <p class="text-xs text-warning">{t.y2jb.regionMissingYouTube}</p>
        {/if}

        <div class="flex flex-wrap items-end gap-3">
          <div class="flex min-w-0 flex-col gap-1.5">
            <Label for={`release-${row.title_id}`}>{t.y2jb.release}</Label>
            <Select
              type="single"
              value={row.selectedTag}
              items={installer.releaseItems}
              onValueChange={(tag) => installer.setTag(row.region, tag)}
            >
              <SelectTrigger id={`release-${row.title_id}`} class="w-full sm:w-64">
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

          <div class="flex flex-wrap gap-2">
            <Button
              size="sm"
              variant="outline"
              disabled={!installer.desktop ||
                installer.busy !== '' ||
                !row.selectedTag ||
                !row.youtubeInstalled}
              loading={installer.busy === 'download0-fetch' && installer.busyRegion === row.region}
              onclick={() => installer.getDownload0(row.region)}
            >
              {t.y2jb.fetch}
            </Button>
            <Button
              size="sm"
              disabled={!installer.targetIp ||
                installer.busy !== '' ||
                !row.cacheMatches ||
                !row.youtubeInstalled}
              loading={installer.busy === 'download0-upload' && installer.busyRegion === row.region}
              onclick={() => installer.sendDownload0(row.region)}
            >
              {t.y2jb.upload}
            </Button>
            {#if row.status?.present}
              <Button
                size="sm"
                variant="outline"
                disabled={!installer.targetIp || installer.busy !== '' || installer.jobRunning}
                onclick={() => installer.removeDownload0File(row.region)}
              >
                {t.y2jb.uninstall}
              </Button>
            {/if}
          </div>
        </div>

        {#if row.selectedRelease}
          <div class="flex flex-wrap items-center gap-2 text-xs text-muted-foreground">
            <span>{formatBytes(row.selectedRelease.size)}</span>
            <span>·</span>
            <span>{t.y2jb.published} {row.selectedRelease.published_at.slice(0, 10)}</span>
            {#if row.selectedIsLatest}
              <Badge class="border-success/30 bg-success/10 text-success">{t.y2jb.latest}</Badge>
            {:else if row.selectedRelease.prerelease}
              <Badge variant="outline">{t.y2jb.prerelease}</Badge>
            {/if}
          </div>
        {/if}

        {#if installer.cache?.download0 && row.selectedTag && !installer.cacheTag}
          <Alert class="border-warning/40 bg-warning/10">
            <AlertDescription class="text-warning">
              {t.y2jb.cacheUnknownRelease(row.selectedTag)}
            </AlertDescription>
          </Alert>
        {:else if installer.cacheTag && row.selectedTag && !row.cacheMatches}
          <Alert class="border-warning/40 bg-warning/10">
            <AlertDescription class="text-warning">
              {t.y2jb.cacheMismatch(installer.cacheTag, row.selectedTag)}
            </AlertDescription>
          </Alert>
        {/if}
      </div>
    {/if}

    <Notice notice={installer.notices.y2jb} />
  </div>
</Step>
