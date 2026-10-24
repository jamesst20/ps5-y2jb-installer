<script>
	import { Select as SelectPrimitive } from "bits-ui";

	let { ref = $bindable(null), ...restProps } = $props();
</script>

<SelectPrimitive.Value bind:ref data-slot="select-value" {...restProps} />