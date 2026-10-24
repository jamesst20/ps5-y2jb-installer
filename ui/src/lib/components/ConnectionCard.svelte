<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<script>
  import { Button } from '$lib/components/ui/button/index.js';
  import { Card, CardContent } from '$lib/components/ui/card/index.js';
  import { Input } from '$lib/components/ui/input/index.js';
  import { Label } from '$lib/components/ui/label/index.js';
  import { Badge } from '$lib/components/ui/badge/index.js';
  import { Alert, AlertDescription } from '$lib/components/ui/alert/index.js';
  import { Progress } from '$lib/components/ui/progress/index.js';
  import { strings as t } from '$lib/strings.js';
  import { formatBytes } from '$lib/format.js';
  import Notice from '$lib/components/Notice.svelte';

  let { installer } = $props();
</script>

<Card class="gap-0 py-0">
  <CardContent class="flex flex-col gap-3 px-4 py-3">
    <div class="flex items-center justify-between">
      <span class="text-sm font-medium">{t.connection.console}</span>
      <Badge
        variant="outline"
        class={installer.reachable ? 'border-success/30 bg-success/10 text-success' : 'text-muted-foreground'}
      >
        {installer.reachable ? t.connection.connected : t.connection.notConnected}
      </Badge>
    </div>

    <div class="flex flex-wrap items-end gap-2">
      <div class="flex min-w-44 flex-1 flex-col gap-1.5">
        <Label for="address">{t.connection.address}</Label>
        <Input
          id="address"
          class="font-mono"
          bind:value={installer.ip}
          placeholder={t.connection.consoleIpPlaceholder}
          onkeydown={(event) => {
            if (event.key === 'Enter') installer.useConsole(installer.ip);
          }}
        />
      </div>
      <Button
        disabled={!installer.ip || installer.connecting || (installer.reachable && installer.ip === installer.connectedIp)}
        loading={installer.connecting}
        onclick={() => installer.useConsole(installer.ip)}
      >
        {installer.connecting
          ? t.connection.connecting
          : installer.reachable && installer.ip === installer.connectedIp
            ? t.connection.connected
            : t.connection.connect}
      </Button>
      <Button variant="outline" disabled={!installer.desktop || installer.scanning} loading={installer.scanning} onclick={installer.scan}>
        {installer.scanning ? t.connection.scanning : t.connection.scan}
      </Button>
    </div>

    <p class="text-sm">
      {#if installer.reachable}
        <span class="text-success">●</span>
        {t.connection.payloadLoaded} · <span class="font-mono">v{installer.status.version}</span>
      {:else if installer.loaderVisible}
        <span class="text-warning">●</span>
        {t.connection.loaderFound}
      {:else if installer.statusError && installer.targetIp}
        <span class="text-muted-foreground">●</span>
        {t.connection.nothingAnswered} <span class="font-mono">{installer.targetIp}</span>
      {:else}
        <span class="text-muted-foreground">●</span>
        <span class="text-muted-foreground">{t.connection.notConnected}</span>
      {/if}
    </p>

    {#if installer.found.length > 0}
      <div class="flex flex-col gap-1">
        <p class="text-xs text-muted-foreground">{t.connection.found}</p>
        {#each installer.found as device (device.ip)}
          <button
            type="button"
            class="flex items-center justify-between rounded-lg border border-border px-3 py-2 text-sm transition-colors hover:bg-muted/50"
            onclick={() => installer.useConsole(device.ip)}
          >
            <span class="font-mono">{device.ip}</span>
            <Badge variant={device.api ? 'default' : 'secondary'}>
              {installer.discoveryLabel(device)}
            </Badge>
          </button>
        {/each}
      </div>
    {/if}

    {#if installer.versionMismatch}
      <Alert class="border-warning/40 bg-warning/10">
        <AlertDescription class="flex flex-wrap items-center justify-between gap-3">
          <span>{t.connection.payloadMismatch(installer.status.version, installer.appVersion)}</span>
          <Button size="sm" variant="outline" disabled={installer.sending} onclick={installer.send}>
            {t.connection.sendPayload}
          </Button>
        </AlertDescription>
      </Alert>
    {/if}

    {#if installer.payload && (!installer.reachable || installer.versionMismatch)}
      <div class="flex flex-wrap items-center justify-between gap-2 rounded-lg border border-border px-3 py-2">
        <span class="text-xs text-muted-foreground">{t.connection.payload} · {formatBytes(installer.payload.size)}</span>
        <Button size="sm" disabled={!installer.ip || installer.sending} loading={installer.sending} onclick={installer.send}>
          {installer.sending ? t.connection.sending : t.connection.sendPayload}
        </Button>
      </div>
    {/if}

    {#if installer.sending}
      <div class="flex flex-col gap-1.5">
        <Progress value={installer.payloadPercent} class="h-1.5" />
        <span class="text-xs tabular-nums text-muted-foreground">
          {formatBytes(installer.sent)} / {formatBytes(installer.total)}
        </span>
      </div>
    {/if}

    {#if !installer.kernelReady && installer.reachable}
      <Alert class="border-warning/40 bg-warning/10">
        <AlertDescription class="text-warning">{t.connection.kernelMissingHint}</AlertDescription>
      </Alert>
    {/if}

    {#if installer.reachable}
      <div class="flex flex-wrap items-center gap-x-4 gap-y-1 text-xs text-muted-foreground">
        <span>{t.connection.firmware} <span class="font-mono text-foreground">{installer.status.firmware}</span></span>
        <span>
          {t.connection.kernelRw}
          <span class={installer.status.kernel_rw ? 'text-success' : 'text-warning'}>
            {installer.status.kernel_rw ? t.connection.kernelOk : t.connection.kernelMissing}
          </span>
        </span>
        <div class="ml-auto flex gap-2">
          <Button variant="outline" size="xs" disabled={installer.busy !== '' || installer.jobRunning} onclick={installer.disconnect}>
            {t.connection.disconnect}
          </Button>
          <Button variant="ghost" size="xs" disabled={installer.busy !== '' || installer.jobRunning} onclick={installer.stopPayload}>
            {t.connection.stopPayload}
          </Button>
        </div>
      </div>
    {/if}

    <Notice notice={installer.notices.connect} />
  </CardContent>
</Card>
