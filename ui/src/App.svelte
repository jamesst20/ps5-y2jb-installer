<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<script>
  import { onMount } from 'svelte';
  import PlayIcon from '@lucide/svelte/icons/play';
  import CheckCircle2Icon from '@lucide/svelte/icons/check-circle-2';
  import { Card, CardContent } from '$lib/components/ui/card/index.js';
  import { Alert, AlertDescription } from '$lib/components/ui/alert/index.js';
  import { strings as t } from '$lib/strings.js';
  import { Installer } from '$lib/installer.svelte.js';
  import ConnectionCard from '$lib/components/ConnectionCard.svelte';
  import SetupStepper from '$lib/components/SetupStepper.svelte';
  import AccountsStep from '$lib/components/AccountsStep.svelte';
  import YoutubeStep from '$lib/components/YoutubeStep.svelte';
  import Y2jbStep from '$lib/components/Y2jbStep.svelte';
  import UpdatesStep from '$lib/components/UpdatesStep.svelte';
  import RebootBar from '$lib/components/RebootBar.svelte';

  const installer = new Installer();

  onMount(() => {
    installer.start();
    return () => installer.stop();
  });
</script>

<div class="mx-auto flex max-w-6xl flex-col gap-6 px-5 pt-8 {installer.rebootPending ? 'pb-28' : 'pb-8'}">
  <header class="flex items-center gap-3 px-1">
    <div class="flex size-9 items-center justify-center rounded-lg bg-primary text-primary-foreground">
      <PlayIcon class="size-4" />
    </div>
    <div>
      <h1 class="text-base font-semibold leading-tight">{t.app.title}</h1>
      <p class="text-xs text-muted-foreground">{t.app.subtitle} · v{installer.appVersion}</p>
    </div>
  </header>

  <ConnectionCard {installer} />

  {#if installer.internetOnline && installer.status?.files?.download0?.present}
    <Alert class="sticky top-2 z-20 border-warning/40 shadow-md">
      <AlertDescription class="text-warning">{t.y2jb.internetDetected}</AlertDescription>
    </Alert>
  {/if}

  <SetupStepper {installer} />

  <main class="flex flex-col gap-3">
    {#if installer.completed === 4}
      <Card class="border-success/30 bg-success/5">
        <CardContent class="flex items-center gap-3">
          <CheckCircle2Icon class="size-5 shrink-0 text-success" />
          <div>
            <p class="text-sm font-medium">{t.app.setupComplete}</p>
            <p class="text-xs text-muted-foreground">{t.app.setupCompleteHint}</p>
          </div>
        </CardContent>
      </Card>
    {/if}

    <AccountsStep {installer} />
    <YoutubeStep {installer} />
    <Y2jbStep {installer} />
    <UpdatesStep {installer} />
  </main>
</div>

<RebootBar {installer} />
