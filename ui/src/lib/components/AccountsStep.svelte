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
  import Step from '$lib/components/Step.svelte';
  import Notice from '$lib/components/Notice.svelte';

  let { installer } = $props();
</script>

<Step
  id="step-accounts"
  number="1"
  title={t.accounts.title}
  transfer={installer.signinTransfer}
  onStop={installer.stopTransfer}
  summary={installer.accountsSummary}
  state={installer.stepState('accounts')}
  locked={!installer.reachable}
  open={installer.reachable && installer.openStep === 'accounts'}
  onOpen={() => installer.selectStep('accounts')}
>
  <div class="flex flex-col gap-3">
    {#if installer.accounts.length === 0}
      <p class="text-sm text-muted-foreground">{t.accounts.noAccounts}</p>
    {:else}
      <div class="flex flex-col gap-1.5">
        <Label for="account">{t.accounts.account}</Label>
        <Select type="single" bind:value={installer.accountSlot} items={installer.accountItems}>
          <SelectTrigger id="account" class="w-full">
            <SelectValue placeholder={t.accounts.account} />
          </SelectTrigger>
          <SelectContent>
            {#each installer.accounts as account (account.slot)}
              <SelectItem value={String(account.slot)} label={account.name}>
                {account.name}
              </SelectItem>
            {/each}
          </SelectContent>
        </Select>
      </div>

      {#if installer.selectedAccount}
        <div class="flex flex-wrap gap-1">
          <Badge variant={installer.selectedAccount.logged_in ? 'secondary' : 'outline'}>
            {installer.selectedAccount.logged_in ? t.accounts.loggedIn : t.accounts.loggedOut}
          </Badge>
          <Badge variant={installer.selectedAccount.activated ? 'secondary' : 'outline'}>
            {installer.selectedAccount.activated ? t.accounts.activated : t.accounts.notActivated}
          </Badge>
          <Badge variant={installer.selectedAccount.signed_in ? 'secondary' : 'outline'}>
            {installer.selectedAccount.signed_in ? t.accounts.signedIn : t.accounts.notSignedIn}
          </Badge>
        </div>

        <div class="flex flex-wrap gap-2">
          <Button
            size="sm"
            disabled={!installer.kernelReady || installer.busy !== '' || !installer.selectedAccount.logged_in || installer.selectedAccount.activated}
            onclick={installer.activateSelected}
          >
            {t.accounts.activate}
          </Button>
          <Button
            size="sm"
            variant="outline"
            loading={installer.busy === 'signin'}
            disabled={!installer.desktop || !installer.kernelReady || installer.busy !== '' || !installer.selectedAccount.logged_in || !installer.selectedAccount.activated || installer.selectedAccount.signed_in}
            onclick={() => installer.fakeSigninFlow(installer.selectedAccount)}
          >
            {t.accounts.fakeSignin}
          </Button>
        </div>

        {#if !installer.selectedAccount.logged_in}
          <p class="text-xs text-muted-foreground">{t.accounts.logInFirst}</p>
        {/if}
      {/if}
    {/if}

    {#if installer.rebootNeeded}
      <Alert class="border-warning/40 bg-warning/10">
        <AlertDescription class="flex items-center justify-between gap-3">
          <span>{t.accounts.rebootNeeded}</span>
          <Button size="sm" variant="outline" onclick={installer.rebootConsole}>
            {t.accounts.reboot}
          </Button>
        </AlertDescription>
      </Alert>
    {/if}

    <Notice notice={installer.notices.accounts} />
  </div>
</Step>
