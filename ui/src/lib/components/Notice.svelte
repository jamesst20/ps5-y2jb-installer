<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<script>
  import { Alert, AlertDescription } from '$lib/components/ui/alert/index.js';

  let { notice } = $props();
</script>

{#if notice}
  <Alert variant={notice.ok ? 'default' : 'destructive'}>
    <AlertDescription>{notice.text}</AlertDescription>
  </Alert>
{/if}
