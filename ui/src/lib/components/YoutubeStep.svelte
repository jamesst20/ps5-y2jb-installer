<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<script>
  import { Button } from '$lib/components/ui/button/index.js';
  import { Badge } from '$lib/components/ui/badge/index.js';
  import { Alert, AlertDescription } from '$lib/components/ui/alert/index.js';
  import { strings as t } from '$lib/strings.js';
  import Step from '$lib/components/Step.svelte';
  import Notice from '$lib/components/Notice.svelte';

  let { installer } = $props();

  const activeInstall = $derived(
    installer.youtubeRows.find((row) => row.installJob?.state === 'running') ?? null,
  );
  const installPercent = $derived(
    activeInstall && activeInstall.installJob.progress >= 0
      ? activeInstall.installJob.progress
      : null,
  );
</script>

<Step
  id="step-youtube"
  number="2"
  title={t.youtube.title}
  transfer={installer.youtubeTransfer}
  phase={activeInstall ? installer.installPhase(activeInstall.installJob) : ''}
  percent={installPercent}
  onStop={installer.stopTransfer}
  summary={installer.youtubeSummary}
  state={installer.stepState('youtube')}
  locked={!installer.reachable}
  open={installer.reachable && installer.openStep === 'youtube'}
  onOpen={() => installer.selectStep('youtube')}
>
  <div class="flex flex-col">
    {#if !installer.build}
      <div class="pb-3">
        <Alert class="border-warning/40 bg-warning/10">
          <AlertDescription class="text-warning">{t.youtube.unsupportedFirmware}</AlertDescription>
        </Alert>
      </div>
    {/if}

    {#if installer.youtubeRows.length > 0}
      <p class="pb-3 text-xs text-muted-foreground">{t.youtube.regionHint}</p>
      <div
        class="-mx-4 overflow-hidden rounded-b-xl border-t border-border {installer.notices.youtube
          ? ''
          : '-mb-4'}"
      >
        <div
          class="hidden border-b border-border bg-muted/30 px-3 py-2 text-[11px] font-medium tracking-wide text-muted-foreground uppercase sm:grid sm:grid-cols-[minmax(0,1fr)_minmax(0,1.1fr)_minmax(0,1.1fr)_9rem] sm:gap-3"
        >
          <span>{t.youtube.region}</span>
          <span>{t.youtube.state}</span>
          <span>{t.youtube.download}</span>
          <span class="text-right">{t.youtube.action}</span>
        </div>

        <div class="divide-y divide-border">
          {#each installer.youtubeRows as row (row.region)}
            <div>
              <div
                class="grid grid-cols-2 gap-x-4 gap-y-2 px-3 py-3 sm:grid-cols-[minmax(0,1fr)_minmax(0,1.1fr)_minmax(0,1.1fr)_9rem] sm:items-center sm:gap-3"
              >
                <div class="min-w-0">
                  <span
                    class="mb-1 block text-[10px] font-medium tracking-wide text-muted-foreground uppercase sm:hidden"
                  >
                    {t.youtube.region}
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
                    {t.youtube.state}
                  </span>
                  {#if row.status?.installed}
                    <Badge class="max-w-full truncate border-success/30 bg-success/10 text-success">
                      {row.status.version || t.youtube.installed}
                    </Badge>
                  {:else}
                    <Badge variant="outline">{t.common.notInstalled}</Badge>
                  {/if}
                </div>

                <div class="min-w-0">
                  <span
                    class="mb-1 block text-[10px] font-medium tracking-wide text-muted-foreground uppercase sm:hidden"
                  >
                    {t.youtube.download}
                  </span>
                  {#if installer.cacheLoading}
                    <span class="text-xs text-muted-foreground">{t.common.computing}</span>
                  {:else if row.cached?.merged}
                    <span class="text-xs font-medium text-success">{t.youtube.downloaded}</span>
                  {:else if row.cached}
                    <div class="flex flex-col items-start gap-1">
                      <span class="text-xs font-medium text-warning">{t.youtube.partial}</span>
                      <Button
                        size="sm"
                        variant="outline"
                        class="w-full sm:w-auto"
                        disabled={!installer.desktop || !installer.build || installer.busy !== ''}
                        loading={installer.busy === 'fetch' && installer.busyRegion === row.region}
                        onclick={() => installer.fetchAndJoin(row.region)}
                      >
                        {t.youtube.download}
                      </Button>
                    </div>
                  {:else}
                    <Button
                      size="sm"
                      variant="outline"
                      class="w-full sm:w-auto"
                      disabled={!installer.desktop || !installer.build || installer.busy !== ''}
                      loading={installer.busy === 'fetch' && installer.busyRegion === row.region}
                      onclick={() => installer.fetchAndJoin(row.region)}
                    >
                      {t.youtube.download}
                    </Button>
                  {/if}
                </div>

                <div class="flex flex-col items-stretch gap-1 sm:items-end">
                  <span
                    class="mb-1 block text-[10px] font-medium tracking-wide text-muted-foreground uppercase sm:hidden"
                  >
                    {t.youtube.action}
                  </span>
                  {#if row.cached?.merged}
                    <Button
                      size="sm"
                      variant={row.alreadyInstalled ? 'outline' : 'default'}
                      class="w-full sm:w-auto"
                      disabled={!installer.targetIp ||
                        !installer.build ||
                        installer.busy !== '' ||
                        installer.jobRunning}
                      loading={installer.busy === 'upload' && installer.busyRegion === row.region}
                      onclick={() => installer.uploadAndInstall(row.region)}
                    >
                      {row.alreadyInstalled ? t.youtube.reinstall : t.youtube.install}
                    </Button>
                  {:else}
                    <Button size="sm" variant="outline" class="w-full sm:w-auto" disabled>
                      {t.youtube.downloadFirst}
                    </Button>
                  {/if}
                </div>
              </div>

              {#if row.installJob?.state === 'error'}
                <div class="border-t border-border bg-muted/20 px-3 py-2.5">
                  <p class="text-sm text-destructive">{row.installJob.message}</p>
                </div>
              {/if}
            </div>
          {/each}
        </div>

        <div
          class="flex flex-col gap-2 border-t border-border bg-muted/30 px-3 py-2.5 sm:flex-row sm:items-center sm:justify-between"
        >
          <p class="min-w-0 break-all text-xs text-muted-foreground">
            {t.youtube.cachedPath}
            <span class="font-mono">{installer.cache?.pkgs_path ?? ''}</span>
          </p>
          <Button
            size="sm"
            variant="outline"
            disabled={installer.busy !== '' || !(installer.cache?.pkgs?.length)}
            onclick={installer.wipeYoutubeCache}
          >
            {t.youtube.deleteCache}
          </Button>
        </div>
      </div>
    {/if}

    {#if installer.notices.youtube}
      <div class="pt-3">
        <Notice notice={installer.notices.youtube} />
      </div>
    {/if}
  </div>
</Step>
