<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<script>
  import { Button } from '$lib/components/ui/button/index.js';
  import { Badge } from '$lib/components/ui/badge/index.js';
  import { Alert, AlertDescription } from '$lib/components/ui/alert/index.js';
  import { Progress } from '$lib/components/ui/progress/index.js';
  import { strings as t } from '$lib/strings.js';
  import { formatBytes } from '$lib/format.js';
  import Step from '$lib/components/Step.svelte';
  import Notice from '$lib/components/Notice.svelte';
  import RegionTabs from '$lib/components/RegionTabs.svelte';

  let { installer } = $props();

  let selectedRegion = $state('');

  const tabs = $derived(
    installer.youtubeRows.map((row) => ({
      value: row.region,
      label: t.youtube.regions[row.region] ?? row.region,
      status: row.status?.installed === true ? 'done' : 'missing',
    })),
  );
  const row = $derived(
    installer.youtubeRows.find((entry) => entry.region === selectedRegion) ??
      installer.youtubeRows[0] ??
      null,
  );
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
    {#if !installer.build}
      <Alert class="border-warning/40 bg-warning/10">
        <AlertDescription class="text-warning">{t.youtube.unsupportedFirmware}</AlertDescription>
      </Alert>
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
      <div class="flex flex-col gap-2">
        <div class="flex flex-wrap items-center gap-2">
          <Badge variant="outline" class="font-mono">{row.title_id}</Badge>
          {#if row.status?.installed}
            <Badge class="border-success/30 bg-success/10 text-success">
              {row.status.version || t.youtube.installed}
            </Badge>
          {:else}
            <Badge variant="outline">{t.common.notInstalled}</Badge>
          {/if}
        </div>

        <div class="flex flex-wrap gap-2">
          <Button
            size="sm"
            variant="outline"
            disabled={!installer.desktop || !installer.build || installer.busy !== ''}
            loading={installer.busy === 'fetch' && installer.busyRegion === row.region}
            onclick={() => installer.fetchAndJoin(row.region)}
          >
            {t.youtube.fetchJoin}
          </Button>
          <Button
            size="sm"
            variant={row.alreadyInstalled ? 'outline' : 'default'}
            disabled={!installer.targetIp ||
              !installer.build ||
              installer.busy !== '' ||
              !row.cached?.merged ||
              installer.jobRunning}
            loading={installer.busy === 'upload' && installer.busyRegion === row.region}
            onclick={() => installer.uploadAndInstall(row.region)}
          >
            {row.alreadyInstalled ? t.youtube.reinstall : t.youtube.install}
          </Button>
        </div>

        <div class="flex flex-col gap-0.5">
          <p
            class="text-xs {installer.cacheLoading
              ? 'text-muted-foreground'
              : row.cached?.merged
                ? 'text-success'
                : 'text-destructive'}"
          >
            {#if installer.cacheLoading}
              {t.common.computing}
            {:else if row.cached?.merged}
              {t.youtube.cached}
            {:else}
              {t.common.notCached}
            {/if}
          </p>
          {#if row.cached?.path}
            <p
              class="break-all font-mono text-[11px] {row.cached.merged
                ? 'text-success/80'
                : 'text-muted-foreground'}"
            >
              {row.cached.path}
            </p>
          {/if}
        </div>

        {#if row.installJob}
          <div class="flex flex-col gap-1.5 rounded-md border p-2.5">
            {#if row.installJob.state === 'running'}
              <div class="flex items-center justify-between gap-2 text-xs">
                <span class="font-medium">{installer.installPhase(row.installJob)}</span>
                {#if row.installJob.progress >= 0}
                  <span class="tabular-nums text-muted-foreground">{row.installJob.progress}%</span>
                {/if}
              </div>
              {#if row.installJob.progress >= 0}
                <Progress value={row.installJob.progress} class="h-1.5" />
              {/if}
              {#if row.installJob.total_bytes > 0}
                <span class="text-[11px] tabular-nums text-muted-foreground">
                  {formatBytes(row.installJob.downloaded_bytes)} / {formatBytes(row.installJob.total_bytes)}
                </span>
              {/if}
              {#if !(row.installJob.progress >= 0)}
                <p class="text-xs text-muted-foreground">{row.installJob.message}</p>
              {/if}
            {:else if row.installJob.state === 'error'}
              <p class="text-sm text-destructive">{row.installJob.message}</p>
            {:else if row.installJob.state === 'done'}
              <p class="text-sm text-success">{row.installJob.message}</p>
            {/if}
          </div>
        {/if}
      </div>
    {/if}

    <Notice notice={installer.notices.youtube} />
  </div>
</Step>
