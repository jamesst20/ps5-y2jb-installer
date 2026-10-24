<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<script>
  import { Alert, AlertDescription } from '$lib/components/ui/alert/index.js';

  let { notice } = $props();
</script>

{#if notice}
  <Alert
    variant={notice.ok ? 'default' : 'destructive'}
    class={notice.ok ? 'border-success/40 bg-success/5 text-success' : ''}
  >
    <AlertDescription class={notice.ok ? 'text-success' : ''}>{notice.text}</AlertDescription>
  </Alert>
{/if}
